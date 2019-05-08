#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "lib/maf.h"
#include "platform.h"
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

static TrackedDeviceIndex_t getDeviceIndex(Device device) {
  switch (device) {
    case DEVICE_HEAD: return HEADSET;
    case DEVICE_HAND: return INVALID_DEVICE;
    case DEVICE_HAND_LEFT: return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);
    case DEVICE_HAND_RIGHT: return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);
    case DEVICE_TRACKER_1:
    case DEVICE_TRACKER_2:
    case DEVICE_TRACKER_3:
    case DEVICE_TRACKER_4: {
      TrackedDeviceIndex_t trackers[4];
      uint32_t trackerCount = state.system->GetSortedTrackedDeviceIndicesOfClass(ETrackedDeviceClass_TrackedDeviceClass_GenericTracker, trackers, 4, 0);
      uint32_t i = device - DEVICE_TRACKER_1;
      return i < trackerCount ? trackers[i] : INVALID_DEVICE;
    }
    default: return INVALID_DEVICE;
  }
}

static bool openvr_getName(char* name, size_t length);
static bool openvr_init(float offset, int msaa) {
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

  openvr_getName(buffer, sizeof(buffer));
  state.rift = !strncmp(buffer, "Oculus", sizeof(buffer));
  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.offset = state.compositor->GetTrackingSpace() == ETrackingUniverseOrigin_TrackingUniverseStanding ? 0. : offset;
  state.msaa = msaa;

  vec_init(&state.boundsGeometry);
  return true;
}

static void openvr_destroy(void) {
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

static bool openvr_getName(char* name, size_t length) {
  ETrackedPropertyError error;
  state.system->GetStringTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_ManufacturerName_String, name, length, &error);
  return error == ETrackedPropertyError_TrackedProp_Success;
}

static HeadsetOrigin openvr_getOriginType(void) {
  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static void openvr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  state.system->GetRecommendedRenderTargetSize(width, height);
}

static double openvr_getDisplayTime(void) {
  float secondsSinceVsync;
  state.system->GetTimeSinceLastVsync(&secondsSinceVsync, NULL);

  float frequency = state.system->GetFloatTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL);
  float frameDuration = 1.f / frequency;
  float vsyncToPhotons = state.system->GetFloatTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, NULL);

  return lovrPlatformGetTime() + (double) (frameDuration - secondsSinceVsync + vsyncToPhotons);
}

static void openvr_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void openvr_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void openvr_getBoundsDimensions(float* width, float* depth) {
  state.chaperone->GetPlayAreaSize(width, depth);
}

static const float* openvr_getBoundsGeometry(int* count) {
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

static bool getTransform(unsigned int index, mat4 transform) {
  TrackedDevicePose_t pose = state.poses[index];
  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    return false;
  } else {
    mat4_fromMat34(transform, pose.mDeviceToAbsoluteTracking.m);
    transform[13] += state.offset;
    return true;
  }
}

static bool openvr_getPose(Device device, vec3 position, quat orientation) {
  float transform[16];
  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index == INVALID_DEVICE || !getTransform(index, transform)) {
    return false;
  }

  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);
  return true;
}

static bool openvr_getBonePose(Device device, DeviceBone bone, vec3 position, quat orientation) {
  return false;
}

static bool openvr_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  TrackedDeviceIndex_t index = getDeviceIndex(device);
  TrackedDevicePose_t* pose = &state.poses[device];
  if (index == INVALID_DEVICE || !pose->bPoseIsValid || !pose->bDeviceIsConnected) {
    return false;
  }

  if (velocity) {
    vec3_init(velocity, pose->vVelocity.v);
  }

  if (angularVelocity) {
    vec3_init(angularVelocity, pose->vAngularVelocity.v);
  }

  return true;
}

static bool openvr_getAcceleration(Device device, vec3 acceleration, vec3 angularAcceleration) {
  return false;
}

static bool getButtonState(Device device, DeviceButton button, bool touch, bool* value) {
  VRControllerState_t input;

  if (device == DEVICE_HEAD && button == BUTTON_PROXIMITY && !touch) {
    if (!state.system->GetControllerState(HEADSET, &input, sizeof(input))) {
      return false;
    }

    return *value = (input.ulButtonPressed >> EVRButtonId_k_EButton_ProximitySensor) & 1, true;
  }

  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index != INVALID_DEVICE || state.system->GetControllerState(index, &input, sizeof(input))) {
    uint64_t mask = touch ? input.ulButtonTouched : input.ulButtonPressed;

    if (state.rift) {
      switch (button) {
        case BUTTON_TRIGGER: return *value = (mask >> EVRButtonId_k_EButton_Axis1) & 1, true;
        case BUTTON_GRIP: return *value = (mask >> EVRButtonId_k_EButton_Axis2) & 1, true;
        case BUTTON_A: return *value = (mask >> EVRButtonId_k_EButton_A) & 1, device == DEVICE_HAND_RIGHT;
        case BUTTON_B: return *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1, device == DEVICE_HAND_RIGHT;
        case BUTTON_X: return *value = (mask >> EVRButtonId_k_EButton_A) & 1, device == DEVICE_HAND_LEFT;
        case BUTTON_Y: return *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1, device == DEVICE_HAND_LEFT;
        default: return false;
      }
    } else {
      switch (button) {
        case BUTTON_TRIGGER: return *value = (mask >> EVRButtonId_k_EButton_SteamVR_Trigger) & 1, true;
        case BUTTON_TRACKPAD: return *value = (mask >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1, true;
        case BUTTON_MENU: return *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1, true;
        case BUTTON_GRIP: return *value = (mask >> EVRButtonId_k_EButton_Grip) & 1, true;
        default: return false;
      }
    }
  }

  return false;
}

static bool openvr_isDown(Device device, DeviceButton button, bool* down) {
  return getButtonState(device, button, false, down);
}

static bool openvr_isTouched(Device device, DeviceButton button, bool* touched) {
  return getButtonState(device, button, true, touched);
}

static bool openvr_getAxis(Device device, DeviceAxis axis, float* value) {
  TrackedDeviceIndex_t index = getDeviceIndex(device);

  VRControllerState_t input;
  if (index != INVALID_DEVICE || state.system->GetControllerState(index, &input, sizeof(input))) {
    switch (axis) {
      case AXIS_TRIGGER: return *value = input.rAxis[1].x, true;
      case AXIS_GRIP: return *value = input.rAxis[2].x, state.rift;
      case AXIS_TRACKPAD_X: return *value = input.rAxis[0].x, !state.rift;
      case AXIS_TRACKPAD_Y: return *value = input.rAxis[0].y, !state.rift;
      case AXIS_THUMBSTICK_X: return *value = input.rAxis[0].x, state.rift;
      case AXIS_THUMBSTICK_Y: return *value = input.rAxis[0].y, state.rift;
      default: return false;
    }
  }

  return false;
}

static bool openvr_vibrate(Device device, float strength, float duration, float frequency) {
  if (duration <= 0) return false;

  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index == INVALID_DEVICE) return false;

  unsigned short uSeconds = (unsigned short) (duration * 1e6f + .5f);
  state.system->TriggerHapticPulse(index, 0, uSeconds);
  return true;
}

static ModelData* openvr_newModelData(Device device) {
  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index == INVALID_DEVICE) return false;

  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(index, renderModelNameProperty, renderModelName, 1024, NULL);

  if (!state.deviceModels[index]) {
    while (state.renderModels->LoadRenderModel_Async(renderModelName, &state.deviceModels[index]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  if (!state.deviceTextures[index]) {
    while (state.renderModels->LoadTexture_Async(state.deviceModels[index]->diffuseTextureId, &state.deviceTextures[index]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  RenderModel_t* vrModel = state.deviceModels[index];
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

  RenderModel_TextureMap_t* vrTexture = state.deviceTextures[index];
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

static void openvr_renderTo(void (*callback)(void*), void* userdata) {
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
  Texture_t eyeTexture = { (void*) id, ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Linear };
  VRTextureBounds_t left = { 0.f, 0.f, .5f, 1.f };
  VRTextureBounds_t right = { .5f, 0.f, 1.f, 1.f };
  state.compositor->Submit(EVREye_Eye_Left, &eyeTexture, &left, EVRSubmitFlags_Submit_Default);
  state.compositor->Submit(EVREye_Eye_Right, &eyeTexture, &right, EVRSubmitFlags_Submit_Default);
  lovrGpuDirtyTexture();
}

static void openvr_update(float dt) {
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

static Texture* openvr_getMirrorTexture(void) {
  return lovrCanvasGetAttachments(state.canvas, NULL)[0].texture;
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  .driverType = DRIVER_OPENVR,
  .init = openvr_init,
  .destroy = openvr_destroy,
  .getName = openvr_getName,
  .getOriginType = openvr_getOriginType,
  .getDisplayDimensions = openvr_getDisplayDimensions,
  .getDisplayTime = openvr_getDisplayTime,
  .getClipDistance = openvr_getClipDistance,
  .setClipDistance = openvr_setClipDistance,
  .getBoundsDimensions = openvr_getBoundsDimensions,
  .getBoundsGeometry = openvr_getBoundsGeometry,
  .getPose = openvr_getPose,
  .getBonePose = openvr_getBonePose,
  .getVelocity = openvr_getVelocity,
  .getAcceleration = openvr_getAcceleration,
  .isDown = openvr_isDown,
  .isTouched = openvr_isTouched,
  .getAxis = openvr_getAxis,
  .vibrate = openvr_vibrate,
  .newModelData = openvr_newModelData,
  .renderTo = openvr_renderTo,
  .getMirrorTexture = openvr_getMirrorTexture,
  .update = openvr_update
};
