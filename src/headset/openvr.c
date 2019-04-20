#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "lib/maf.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef WIN32
#pragma pack(push, 4)
#else
#undef EXTERN_C
#endif
#include <openvr_capi.h>
#ifndef WIN32
#pragma pack(pop)
#endif

// From openvr_capi.h
extern intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
extern void VR_ShutdownInternal();
extern bool VR_IsHmdPresent();
extern intptr_t VR_GetGenericInterface(const char* pchInterfaceVersion, EVRInitError* peError);
extern bool VR_IsRuntimeInstalled();
#define HEADSET k_unTrackedDeviceIndex_Hmd
#define INVALID_DEVICE k_unTrackedDeviceIndexInvalid

static struct {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;
  TrackedDevicePose_t poses[16];
  RenderModel_t* deviceModels[16];
  RenderModel_TextureMap_t* deviceTextures[16];
  Canvas* canvas;
  vec_float_t boundsGeometry;
  bool rift;
  float clipNear;
  float clipFar;
  float offset;
  int msaa;
} state;

static TrackedDeviceIndex_t pathToDevice(const char* path, const char** next) {
  if (!strcmp(path, "head")) {
    if (next) *next = path + strlen("head/");
    return HEADSET;
  } else if (!strcmp(path, "hand/left")) {
    if (next) *next = path + strlen("hand/left/");
    return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);
  } else if (!strcmp(path, "hand/right")) {
    if (next) *next = path + strlen("hand/right/");
    return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);
  } else if (!strncmp("tracker/", path, strlen("tracker/"))) {
    path += strlen("tracker/");
    uint32_t index = (path[0] - '0') - 1;
    TrackedDeviceIndex_t indices[8];
    uint32_t trackerCount = state.system->GetSortedTrackedDeviceIndicesOfClass(ETrackedDeviceClass_TrackedDeviceClass_GenericTracker, indices, 8, 0);
    return (index >= 0 && index < trackerCount) ? indices[index] : INVALID_DEVICE;
  } else {
    return INVALID_DEVICE;
  }
}

static bool getName(char* name, size_t length);
static bool init(float offset, int msaa) {
  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    return false;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);
  if (vrError != EVRInitError_VRInitError_None) {
    return false;
  }

  char buffer[64];
  sprintf(buffer, "FnTable:%s", IVRSystem_Version), state.system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRCompositor_Version), state.compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRChaperone_Version), state.chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRRenderModels_Version), state.renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(buffer, &vrError);

  if (!state.system || !state.compositor || !state.chaperone || !state.renderModels) {
    VR_ShutdownInternal();
    return false;
  }

  getName(buffer, sizeof(buffer));
  state.rift = !strncmp(buffer, "Oculus", sizeof(buffer));
  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.offset = state.compositor->GetTrackingSpace() == ETrackingUniverseOrigin_TrackingUniverseStanding ? 0. : offset;
  state.msaa = msaa;

  vec_init(&state.boundsGeometry);
  return true;
}

static void destroy(void) {
  lovrRelease(Canvas, state.canvas);
  for (int i = 0; i < 16; i++) {
    if (state.deviceModels[i]) {
      state.renderModels->FreeRenderModel(state.deviceModels[i]);
    }
    if (state.deviceTextures[i]) {
      state.renderModels->FreeTexture(state.deviceTextures[i]);
    }
    state.deviceModels[i] = NULL;
    state.deviceTextures[i] = NULL;
  }
  vec_deinit(&state.boundsGeometry);
  VR_ShutdownInternal();
  memset(&state, 0, sizeof(state));
}

static bool getName(char* name, size_t length) {
  ETrackedPropertyError error;
  state.system->GetStringTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_ManufacturerName_String, name, length, &error);
  return error == ETrackedPropertyError_TrackedProp_Success;
}

static HeadsetOrigin getOriginType(void) {
  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static void getDisplayDimensions(uint32_t* width, uint32_t* height) {
  state.system->GetRecommendedRenderTargetSize(width, height);
}

static void getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void getBoundsDimensions(float* width, float* depth) {
  state.chaperone->GetPlayAreaSize(width, depth);
}

static const float* getBoundsGeometry(int* count) {
  struct HmdQuad_t quad;
  if (state.chaperone->GetPlayAreaRect(&quad)) {
    vec_clear(&state.boundsGeometry);

    for (int i = 0; i < 4; i++) {
      vec_push(&state.boundsGeometry, quad.vCorners[i].v[0]);
      vec_push(&state.boundsGeometry, quad.vCorners[i].v[1]);
      vec_push(&state.boundsGeometry, quad.vCorners[i].v[2]);
    }

    *count = state.boundsGeometry.length;
    return state.boundsGeometry.data;
  }

  return NULL;
}

static bool getTransform(unsigned int device, mat4 transform) {
  TrackedDevicePose_t pose = state.poses[device];
  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    return false;
  } else {
    mat4_fromMat34(transform, pose.mDeviceToAbsoluteTracking.m);
    transform[13] += state.offset;
    return true;
  }
}

static bool getPose(const char* path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  float transform[16];
  TrackedDeviceIndex_t device = pathToDevice(path, NULL);
  if (device == INVALID_DEVICE && !getTransform(device, transform)) {
    return false;
  }

  mat4_getTransform(transform, x, y, z, NULL, NULL, NULL, angle, ax, ay, az);
  return true;
}

static bool getVelocity(const char* path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz) {
  TrackedDeviceIndex_t device = pathToDevice(path, NULL);
  TrackedDevicePose_t* pose = &state.poses[device];
  if (device == INVALID_DEVICE || !pose->bPoseIsValid || !pose->bDeviceIsConnected) {
    return false;
  }

  if (vx) {
    *vx = pose->vVelocity.v[0];
    *vy = pose->vVelocity.v[1];
    *vz = pose->vVelocity.v[2];
  }

  if (vax) {
    *vax = pose->vAngularVelocity.v[0];
    *vay = pose->vAngularVelocity.v[1];
    *vaz = pose->vAngularVelocity.v[2];
  }

  return true;
}

static bool getButtonState(const char* path, bool touch, bool* value) {
  VRControllerState_t input;

  if (!strcmp(path, "head/proximity") && !touch) {
    if (!state.system->GetControllerState(HEADSET, &input, sizeof(input))) {
      return false;
    }

    return *value = (input.ulButtonPressed >> EVRButtonId_k_EButton_ProximitySensor) & 1, true;
  }

  const char* button;
  TrackedDeviceIndex_t device = pathToDevice(path, &button);
  if (device != INVALID_DEVICE || state.system->GetControllerState(device, &input, sizeof(input))) {
    uint64_t mask = touch ? input.ulButtonTouched : input.ulButtonPressed;

    if (state.rift) {
      if (!strcmp(button, "trigger")) { return *value = (mask >> EVRButtonId_k_EButton_Axis1) & 1, true; }
      else if (!strcmp(button, "grip")) { return *value = (mask >> EVRButtonId_k_EButton_Axis2) & 1, true; }
      else if (!strcmp(button, "trackpad")) { return *value = (mask >> EVRButtonId_k_EButton_Axis0) & 1, true; }
      else if (!strcmp(path, "hand/right/a") || !strcmp(path, "hand/left/x")) { return *value = (mask >> EVRButtonId_k_EButton_A) & 1, true; }
      else if (!strcmp(path, "hand/right/b") || !strcmp(path, "hand/left/y")) { return *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1, true; }
    } else {
      if (!strcmp(button, "trigger")) { return *value = (mask >> EVRButtonId_k_EButton_SteamVR_Trigger) & 1, true; }
      else if (!strcmp(button, "trackpad")) { return *value = (mask >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1, true; }
      else if (!strcmp(button, "menu")) { return *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1, true; }
      else if (!strcmp(button, "grip")) { return *value = (mask >> EVRButtonId_k_EButton_Grip) & 1, true; }
    }
  }

  return false;
}

static bool isDown(const char* path, bool* down) {
  return getButtonState(path, false, down);
}

static bool isTouched(const char* path, bool* touched) {
  return getButtonState(path, true, touched);
}

static int getAxis(const char* path, float* x, float* y, float* z) {
  const char* axis;
  TrackedDeviceIndex_t device = pathToDevice(path, &axis);

  VRControllerState_t input;
  if (device != INVALID_DEVICE || state.system->GetControllerState(device, &input, sizeof(input))) {
    if (!strcmp(axis, "trigger")) { return *x = input.rAxis[1].x, 1; }
    else if (state.rift && !strcmp(axis, "grip")) { return *x = input.rAxis[2].x, 1; }
    else if (!strcmp(axis, "trackpad")) { return *x = input.rAxis[0].x, *y = input.rAxis[0].y, 2; }
  }

  return 0;
}

static bool vibrate(const char* path, float strength, float duration, float frequency) {
  if (duration <= 0) return false;

  TrackedDeviceIndex_t device = pathToDevice(path, NULL);
  if (device == INVALID_DEVICE) return false;

  unsigned short uSeconds = (unsigned short) (duration * 1e6f + .5f);
  state.system->TriggerHapticPulse(device, 0, uSeconds);
  return true;
}

static ModelData* newModelData(const char* path) {
  TrackedDeviceIndex_t device = pathToDevice(path, NULL);
  if (device == INVALID_DEVICE) return false;

  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(device, renderModelNameProperty, renderModelName, 1024, NULL);

  if (!state.deviceModels[device]) {
    while (state.renderModels->LoadRenderModel_Async(renderModelName, &state.deviceModels[device]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  if (!state.deviceTextures[device]) {
    while (state.renderModels->LoadTexture_Async(state.deviceModels[device]->diffuseTextureId, &state.deviceTextures[device]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  RenderModel_t* vrModel = state.deviceModels[device];
  ModelData* model = lovrAlloc(ModelData);
  size_t vertexSize = sizeof(RenderModel_Vertex_t);

  model->bufferCount = 2;
  model->attributeCount = 4;
  model->textureCount = 1;
  model->materialCount = 1;
  model->primitiveCount = 1;
  model->nodeCount = 1;
  lovrModelDataAllocate(model);

  model->buffers[0] = (ModelBuffer) {
    .data = (char*) vrModel->rVertexData,
    .size = vrModel->unVertexCount * vertexSize,
    .stride = vertexSize
  };

  model->buffers[1] = (ModelBuffer) {
    .data = (char*) vrModel->rIndexData,
    .size = vrModel->unTriangleCount * 3 * sizeof(uint16_t),
    .stride = sizeof(uint16_t)
  };

  model->attributes[0] = (ModelAttribute) {
    .buffer = 0,
    .offset = offsetof(RenderModel_Vertex_t, vPosition),
    .count = vrModel->unVertexCount,
    .type = F32,
    .components = 3
  };

  model->attributes[1] = (ModelAttribute) {
    .buffer = 0,
    .offset = offsetof(RenderModel_Vertex_t, vNormal),
    .count = vrModel->unVertexCount,
    .type = F32,
    .components = 3
  };

  model->attributes[2] = (ModelAttribute) {
    .buffer = 0,
    .offset = offsetof(RenderModel_Vertex_t, rfTextureCoord),
    .count = vrModel->unVertexCount,
    .type = F32,
    .components = 2
  };

  model->attributes[3] = (ModelAttribute) {
    .buffer = 1,
    .offset = 0,
    .count = vrModel->unTriangleCount * 3,
    .type = U16,
    .components = 1
  };

  RenderModel_TextureMap_t* vrTexture = state.deviceTextures[device];
  model->textures[0] = lovrTextureDataCreate(vrTexture->unWidth, vrTexture->unHeight, 0, FORMAT_RGBA);
  memcpy(model->textures[0]->blob.data, vrTexture->rubTextureMapData, vrTexture->unWidth * vrTexture->unHeight * 4);

  model->materials[0] = (ModelMaterial) {
    .colors[COLOR_DIFFUSE] = { 1.f, 1.f, 1.f, 1.f },
    .textures[TEXTURE_DIFFUSE] = 0,
    .filters[TEXTURE_DIFFUSE] = lovrGraphicsGetDefaultFilter()
  };

  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLES,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1],
      [ATTR_TEXCOORD] = &model->attributes[2]
    },
    .indices = &model->attributes[3],
    .material = 0
  };

  model->nodes[0] = (ModelNode) {
    .transform = MAT4_IDENTITY,
    .primitiveIndex = 0,
    .primitiveCount = 1
  };

  return model;
}

static void renderTo(void (*callback)(void*), void* userdata) {
  if (!state.canvas) {
    uint32_t width, height;
    state.system->GetRecommendedRenderTargetSize(&width, &height);
    CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = true, .msaa = state.msaa };
    state.canvas = lovrCanvasCreate(width * 2, height, flags);
    Texture* texture = lovrTextureCreate(TEXTURE_2D, NULL, 0, true, true, state.msaa);
    lovrTextureAllocate(texture, width * 2, height, 1, FORMAT_RGBA);
    lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
    lovrCanvasSetAttachments(state.canvas, &(Attachment) { texture, 0, 0 }, 1);
    lovrRelease(Texture, texture);
  }

  Camera camera = { .canvas = state.canvas, .viewMatrix = { MAT4_IDENTITY, MAT4_IDENTITY } };

  float head[16], eye[16];
  getTransform(HEADSET, head);

  for (int i = 0; i < 2; i++) {
    EVREye vrEye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;
    mat4_fromMat44(camera.projection[i], state.system->GetProjectionMatrix(vrEye, state.clipNear, state.clipFar).m);
    mat4_multiply(camera.viewMatrix[i], head);
    mat4_multiply(camera.viewMatrix[i], mat4_fromMat34(eye, state.system->GetEyeToHeadTransform(vrEye).m));
    mat4_invertPose(camera.viewMatrix[i]);
  }

  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);

  // Submit
  const Attachment* attachments = lovrCanvasGetAttachments(state.canvas, NULL);
  ptrdiff_t id = attachments[0].texture->id;
  EColorSpace colorSpace = lovrGraphicsIsGammaCorrect() ? EColorSpace_ColorSpace_Linear : EColorSpace_ColorSpace_Gamma;
  Texture_t eyeTexture = { (void*) id, ETextureType_TextureType_OpenGL, colorSpace };
  VRTextureBounds_t left = { 0.f, 0.f, .5f, 1.f };
  VRTextureBounds_t right = { .5f, 0.f, 1.f, 1.f };
  state.compositor->Submit(EVREye_Eye_Left, &eyeTexture, &left, EVRSubmitFlags_Submit_Default);
  state.compositor->Submit(EVREye_Eye_Right, &eyeTexture, &right, EVRSubmitFlags_Submit_Default);
  lovrGpuDirtyTexture();
}

static void update(float dt) {
  state.compositor->WaitGetPoses(state.poses, 16, NULL, 0);

  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_InputFocusCaptured:
      case EVREventType_VREvent_InputFocusReleased: {
        bool isFocused = vrEvent.eventType == EVREventType_VREvent_InputFocusReleased;
        lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { isFocused } });
        break;
      }

      default: break;
    }
  }
}

static Texture* getMirrorTexture(void) {
  return lovrCanvasGetAttachments(state.canvas, NULL)[0].texture;
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  .driverType = DRIVER_OPENVR,
  .init = init,
  .destroy = destroy,
  .getName = getName,
  .getOriginType = getOriginType,
  .getDisplayDimensions = getDisplayDimensions,
  .getClipDistance = getClipDistance,
  .setClipDistance = setClipDistance,
  .getBoundsDimensions = getBoundsDimensions,
  .getBoundsGeometry = getBoundsGeometry,
  .getPose = getPose,
  .getVelocity = getVelocity,
  .isDown = isDown,
  .isTouched = isTouched,
  .getAxis = getAxis,
  .vibrate = vibrate,
  .newModelData = newModelData,
  .renderTo = renderTo,
  .getMirrorTexture = getMirrorTexture,
  .update = update
};
