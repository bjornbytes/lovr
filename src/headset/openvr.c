#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "platform.h"
#include "util.h"
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
#define HEADSET_INDEX k_unTrackedDeviceIndex_Hmd
#define INVALID_INDEX k_unTrackedDeviceIndexInvalid

static ControllerHand openvrControllerGetHand(Controller* controller);

typedef struct {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;
  TrackedDevicePose_t poses[16];
  RenderModel_t* deviceModels[16];
  RenderModel_TextureMap_t* deviceTextures[16];
  Canvas* canvas;
  vec_float_t boundsGeometry;
  vec_controller_t controllers;
  HeadsetType type;
  float clipNear;
  float clipFar;
  float offset;
  int msaa;
} HeadsetState;

static HeadsetState state;

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

static TrackedDeviceIndex_t getDeviceIndexForPath(Path path) {
  if (PATH_EQ(path, PATH_HEAD)) {
    return HEADSET_INDEX;
  } else if (PATH_EQ(path, PATH_HANDS, PATH_LEFT)) {
    return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);
  } else if (PATH_EQ(path, PATH_HANDS, PATH_RIGHT)) {
    return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);
  } else {
    return k_unTrackedDeviceIndexInvalid;
  }
}

static bool isController(TrackedDeviceIndex_t id) {
  return state.system->IsTrackedDeviceConnected(id) &&
    (state.system->GetTrackedDeviceClass(id) == ETrackedDeviceClass_TrackedDeviceClass_Controller ||
    state.system->GetTrackedDeviceClass(id) == ETrackedDeviceClass_TrackedDeviceClass_GenericTracker);
}

static ControllerButton getButton(uint32_t button, ControllerHand hand) {
  switch (state.type) {
    case HEADSET_RIFT:
      switch (button) {
        case EVRButtonId_k_EButton_Axis1: return CONTROLLER_BUTTON_TRIGGER;
        case EVRButtonId_k_EButton_Axis2: return CONTROLLER_BUTTON_GRIP;
        case EVRButtonId_k_EButton_Axis0: return CONTROLLER_BUTTON_TOUCHPAD;
        case EVRButtonId_k_EButton_A:
          switch (hand) {
            case HAND_LEFT: return CONTROLLER_BUTTON_X;
            case HAND_RIGHT: return CONTROLLER_BUTTON_A;
            default: return CONTROLLER_BUTTON_UNKNOWN;
          }
        case EVRButtonId_k_EButton_ApplicationMenu:
          switch (hand) {
            case HAND_LEFT: return CONTROLLER_BUTTON_Y;
            case HAND_RIGHT: return CONTROLLER_BUTTON_B;
            default: return CONTROLLER_BUTTON_UNKNOWN;
          }
        default: return CONTROLLER_BUTTON_UNKNOWN;
      }
      break;

    default:
      switch (button) {
        case EVRButtonId_k_EButton_System: return CONTROLLER_BUTTON_SYSTEM;
        case EVRButtonId_k_EButton_ApplicationMenu: return CONTROLLER_BUTTON_MENU;
        case EVRButtonId_k_EButton_SteamVR_Trigger: return CONTROLLER_BUTTON_TRIGGER;
        case EVRButtonId_k_EButton_Grip: return CONTROLLER_BUTTON_GRIP;
        case EVRButtonId_k_EButton_SteamVR_Touchpad: return CONTROLLER_BUTTON_TOUCHPAD;
        default: return CONTROLLER_BUTTON_UNKNOWN;
      }
  }

  return CONTROLLER_BUTTON_UNKNOWN;
}

static bool getButtonState(Path path, bool touch, bool* value) {
  if (!PATH_EQ(path, PATH_HANDS, PATH_LEFT) && !PATH_EQ(path, PATH_HANDS, PATH_RIGHT)) {
    return false;
  }

  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) {
    return false;
  }

  VRControllerState_t input;
  if (!state.system->GetControllerState(deviceIndex, &input, sizeof(input))) {
    return false;
  }

  uint64_t mask = touch ? input.ulButtonTouched : input.ulButtonPressed;

  ControllerHand hand = HAND_UNKNOWN;
  switch (state.system->GetControllerRoleForTrackedDeviceIndex(deviceIndex)) {
    case ETrackedControllerRole_TrackedControllerRole_LeftHand: return HAND_LEFT;
    case ETrackedControllerRole_TrackedControllerRole_RightHand: return HAND_RIGHT;
    default: break;
  }

  switch (state.type) {
    case HEADSET_RIFT:
      switch (path.pieces[2]) {
        case PATH_TRIGGER:
          *value = (mask >> EVRButtonId_k_EButton_Axis1) & 1;
          return true;

        case PATH_GRIP:
          *value = (mask >> EVRButtonId_k_EButton_Axis2) & 1;
          return true;

        case PATH_TRACKPAD:
          *value = (mask >> EVRButtonId_k_EButton_Axis0) & 1;
          return true;

        case PATH_A:
          *value = hand == HAND_RIGHT && (mask >> EVRButtonId_k_EButton_A) & 1;
          return true;

        case PATH_B:
          *value = hand == HAND_RIGHT && (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
          return true;

        case PATH_X:
          *value = hand == HAND_LEFT && (mask >> EVRButtonId_k_EButton_A) & 1;
          return true;

        case PATH_Y:
          *value = hand == HAND_LEFT && (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
          return true;

        default: return false;
      }

    default:
      switch (path.pieces[2]) {
        case PATH_TRIGGER:
          *value = (mask >> EVRButtonId_k_EButton_SteamVR_Trigger) & 1;
          return true;

        case PATH_TRACKPAD:
          *value = (mask >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1;
          return true;

        case PATH_MENU:
          *value = (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
          return true;

        case PATH_GRIP:
          *value = (mask >> EVRButtonId_k_EButton_Grip) & 1;
          return true;

        default: return false;
      }
  }
}

static bool openvrInit(float offset, int msaa) {
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

  state.system->GetStringTrackedDeviceProperty(HEADSET_INDEX, ETrackedDeviceProperty_Prop_ManufacturerName_String, buffer, 128, NULL);

  if (!strncmp(buffer, "HTC", 128)) {
    state.type = HEADSET_VIVE;
  } else if (!strncmp(buffer, "Oculus", 128)) {
    state.type = HEADSET_RIFT;
  } else if (!strncmp(buffer, "WindowsMR", 128)) {
    state.type = HEADSET_WINDOWS_MR;
  } else {
    state.type = HEADSET_UNKNOWN;
  }

  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.offset = state.compositor->GetTrackingSpace() == ETrackingUniverseOrigin_TrackingUniverseStanding ? 0. : offset;
  state.msaa = msaa;

  vec_init(&state.boundsGeometry);
  vec_init(&state.controllers);
  for (uint32_t i = 0; i < k_unMaxTrackedDeviceCount; i++) {
    if (isController(i)) {
      Controller* controller = lovrAlloc(Controller);
      controller->id = i;
      vec_push(&state.controllers, controller);
    }
  }

  return true;
}

static void openvrDestroy(void) {
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
  Controller* controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(Controller, controller);
  }
  vec_deinit(&state.boundsGeometry);
  vec_deinit(&state.controllers);
  VR_ShutdownInternal();
  memset(&state, 0, sizeof(HeadsetState));
}

static HeadsetType openvrGetType(void) {
  return state.type;
}

static HeadsetOrigin openvrGetOriginType(void) {
  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static bool openvrIsMounted(void) {
  VRControllerState_t input;
  state.system->GetControllerState(HEADSET_INDEX, &input, sizeof(input));
  return (input.ulButtonPressed >> EVRButtonId_k_EButton_ProximitySensor) & 1;
}

static void openvrGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  state.system->GetRecommendedRenderTargetSize(width, height);
}

static void openvrGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void openvrSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void openvrGetBoundsDimensions(float* width, float* depth) {
  state.chaperone->GetPlayAreaSize(width, depth);
}

static const float* openvrGetBoundsGeometry(int* count) {
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

static bool openvrGetPose(Path path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  float transform[16];
  if (deviceIndex != INVALID_INDEX && getTransform(deviceIndex, transform)) {
    mat4_getPose(transform, x, y, z, angle, ax, ay, az);
    return true;
  }
  return false;
}

static bool openvrGetVelocity(Path path, float* vx, float* vy, float* vz) {
  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) {
    return false;
  }

  TrackedDevicePose_t* pose = &state.poses[deviceIndex];
  if (!pose->bPoseIsValid || !pose->bDeviceIsConnected) {
    return false;
  } else {
    *vx = pose->vVelocity.v[0];
    *vy = pose->vVelocity.v[1];
    *vz = pose->vVelocity.v[2];
    return true;
  }
}

static bool openvrGetAngularVelocity(Path path, float* vx, float* vy, float* vz) {
  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) {
    return false;
  }

  TrackedDevicePose_t* pose = &state.poses[deviceIndex];
  if (!pose->bPoseIsValid || !pose->bDeviceIsConnected) {
    return false;
  } else {
    *vx = pose->vAngularVelocity.v[0];
    *vy = pose->vAngularVelocity.v[1];
    *vz = pose->vAngularVelocity.v[2];
    return true;
  }
}

static bool openvrIsDown(Path path, bool* down) {
  return getButtonState(path, false, down);
}

static bool openvrIsTouched(Path path, bool* touched) {
  return getButtonState(path, true, touched);
}

static int openvrGetAxis(Path path, float* x, float* y, float* z) {
  if (path.pieces[3] != PATH_NONE) {
    return 0;
  }

  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) {
    return 0;
  }

  VRControllerState_t input;
  if (!state.system->GetControllerState(deviceIndex, &input, sizeof(input))) {
    return 0;
  }

  switch (state.type) {
    case HEADSET_RIFT:
      switch (path.pieces[2]) {
        case PATH_TRIGGER:
          *x = input.rAxis[1].x;
          return 1;

        case PATH_GRIP:
          *x = input.rAxis[2].x;
          return 1;

        case PATH_TRACKPAD:
          *x = input.rAxis[0].x;
          *y = input.rAxis[0].y;
          return 2;

        default: return 0;
      }

    default:
      switch (path.pieces[2]) {
        case PATH_TRIGGER:
          *x = input.rAxis[1].x;
          return 1;

        case PATH_TRACKPAD:
          *x = input.rAxis[0].x;
          *y = input.rAxis[0].y;
          return 2;

        default: return 0;
      }
  }

  return 0;
}

static bool openvrVibrate(Path path, float strength, float duration, float frequency) {
  if (duration <= 0) return false;
  if (!PATH_STARTS_WITH(path, PATH_HANDS)) return false;

  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) return false;

  unsigned short uSeconds = (unsigned short) (duration * 1e6f);
  state.system->TriggerHapticPulse(deviceIndex, 0, uSeconds);
  return true;
}

static ModelData* openvrNewModelData(Path path) {
  TrackedDeviceIndex_t deviceIndex = getDeviceIndexForPath(path);
  if (deviceIndex == INVALID_INDEX) return false;

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(deviceIndex, renderModelNameProperty, renderModelName, 1024, NULL);

  // Load model
  if (!state.deviceModels[deviceIndex]) {
    while (state.renderModels->LoadRenderModel_Async(renderModelName, &state.deviceModels[deviceIndex]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  // Load texture
  if (!state.deviceTextures[deviceIndex]) {
    while (state.renderModels->LoadTexture_Async(state.deviceModels[deviceIndex]->diffuseTextureId, &state.deviceTextures[deviceIndex]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  RenderModel_t* vrModel = state.deviceModels[deviceIndex];
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

  RenderModel_TextureMap_t* vrTexture = state.deviceTextures[deviceIndex];
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

static Controller** openvrGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

static bool openvrControllerIsConnected(Controller* controller) {
  return state.system->IsTrackedDeviceConnected(controller->id);
}

static ControllerHand openvrControllerGetHand(Controller* controller) {
  switch (state.system->GetControllerRoleForTrackedDeviceIndex(controller->id)) {
    case ETrackedControllerRole_TrackedControllerRole_LeftHand: return HAND_LEFT;
    case ETrackedControllerRole_TrackedControllerRole_RightHand: return HAND_RIGHT;
    default: return HAND_UNKNOWN;
  }
}

static void openvrRenderTo(void (*callback)(void*), void* userdata) {
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
  getTransform(HEADSET_INDEX, head);

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

static void openvrUpdate(float dt) {
  state.compositor->WaitGetPoses(state.poses, 16, NULL, 0);

  // Poll for new events after waiting for poses (to get as many events as possible)
  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_TrackedDeviceActivated: {
        uint32_t id = vrEvent.trackedDeviceIndex;
        if (isController(id)) {
          Controller* controller = lovrAlloc(Controller);
          controller->id = id;
          vec_push(&state.controllers, controller);
          lovrRetain(controller);
          lovrEventPush((Event) { .type = EVENT_CONTROLLER_ADDED, .data.controller = { controller, 0 } });
        }
        break;
      }

      case EVREventType_VREvent_TrackedDeviceDeactivated:
        for (int i = 0; i < state.controllers.length; i++) {
          Controller* controller = state.controllers.data[i];
          if (controller->id == vrEvent.trackedDeviceIndex) {
            lovrRetain(controller);
            lovrEventPush((Event) { .type = EVENT_CONTROLLER_REMOVED, .data.controller = { controller, 0 } });
            vec_swapsplice(&state.controllers, i, 1);
            lovrRelease(Controller, controller);
            break;
          }
        }
        break;

      case EVREventType_VREvent_ButtonPress:
      case EVREventType_VREvent_ButtonUnpress: {
        bool isPress = vrEvent.eventType == EVREventType_VREvent_ButtonPress;

        if (vrEvent.trackedDeviceIndex == HEADSET_INDEX && vrEvent.data.controller.button == EVRButtonId_k_EButton_ProximitySensor) {
          lovrEventPush((Event) { .type = EVENT_MOUNT, .data.boolean = { isPress } });
          break;
        }

        for (int i = 0; i < state.controllers.length; i++) {
          Controller* controller = state.controllers.data[i];
          if (controller->id == vrEvent.trackedDeviceIndex) {
            ControllerButton button = getButton(vrEvent.data.controller.button, openvrControllerGetHand(controller));
            EventType type = isPress ? EVENT_CONTROLLER_PRESSED : EVENT_CONTROLLER_RELEASED;
            lovrEventPush((Event) { .type = type, .data.controller = { controller, button } });
            break;
          }
        }
        break;
      }

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

static Texture* openvrGetMirrorTexture(void) {
  return lovrCanvasGetAttachments(state.canvas, NULL)[0].texture;
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  .driverType = DRIVER_OPENVR,
  .init = openvrInit,
  .destroy = openvrDestroy,
  .getType = openvrGetType,
  .getOriginType = openvrGetOriginType,
  .isMounted = openvrIsMounted,
  .getDisplayDimensions = openvrGetDisplayDimensions,
  .getClipDistance = openvrGetClipDistance,
  .setClipDistance = openvrSetClipDistance,
  .getBoundsDimensions = openvrGetBoundsDimensions,
  .getBoundsGeometry = openvrGetBoundsGeometry,
  .getPose = openvrGetPose,
  .getVelocity = openvrGetVelocity,
  .getAngularVelocity = openvrGetAngularVelocity,
  .isDown = openvrIsDown,
  .isTouched = openvrIsTouched,
  .getAxis = openvrGetAxis,
  .vibrate = openvrVibrate,
  .newModelData = openvrNewModelData,
  .getControllers = openvrGetControllers,
  .controllerIsConnected = openvrControllerIsConnected,
  .controllerGetHand = openvrControllerGetHand,
  .renderTo = openvrRenderTo,
  .getMirrorTexture = openvrGetMirrorTexture,
  .update = openvrUpdate
};
