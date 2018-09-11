#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "math/mat4.h"
#include "math/quat.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef WIN32
#pragma pack(push, 4)
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

static ControllerHand openvrControllerGetHand(Controller* controller);

typedef struct {
  bool isRendering;
  bool isMirrored;
  float offset;
  int msaa;

  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;

  unsigned int headsetIndex;
  HeadsetType type;

  TrackedDevicePose_t renderPoses[16];
  RenderModel_t* deviceModels[16];
  RenderModel_TextureMap_t* deviceTextures[16];

  vec_controller_t controllers;

  float clipNear;
  float clipFar;

  float refreshRate;
  float vsyncToPhotons;

  Canvas* canvas;
} HeadsetState;

static HeadsetState state;

static bool openvrIsController(TrackedDeviceIndex_t id) {
  if (!state.system->IsTrackedDeviceConnected(id)) {
    return false;
  }

  switch (state.system->GetTrackedDeviceClass(id)) {
    case ETrackedDeviceClass_TrackedDeviceClass_Controller:
    case ETrackedDeviceClass_TrackedDeviceClass_GenericTracker:
      return true;
    default:
      return false;
  }
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

static int getButtonState(uint64_t mask, ControllerButton button, ControllerHand hand) {
  switch (state.type) {
    case HEADSET_RIFT:
      switch (button) {
        case CONTROLLER_BUTTON_TRIGGER: return (mask >> EVRButtonId_k_EButton_Axis1) & 1;
        case CONTROLLER_BUTTON_GRIP: return (mask >> EVRButtonId_k_EButton_Axis2) & 1;
        case CONTROLLER_BUTTON_TOUCHPAD: return (mask >> EVRButtonId_k_EButton_Axis0) & 1;
        case CONTROLLER_BUTTON_A: return hand == HAND_RIGHT && (mask >> EVRButtonId_k_EButton_A) & 1;
        case CONTROLLER_BUTTON_B: return hand == HAND_RIGHT && (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
        case CONTROLLER_BUTTON_X: return hand == HAND_LEFT && (mask >> EVRButtonId_k_EButton_A) & 1;
        case CONTROLLER_BUTTON_Y: return hand == HAND_LEFT && (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
        default: return 0;
      }

    default:
      switch (button) {
        case CONTROLLER_BUTTON_SYSTEM: return (mask >> EVRButtonId_k_EButton_System) & 1;
        case CONTROLLER_BUTTON_MENU: return (mask >> EVRButtonId_k_EButton_ApplicationMenu) & 1;
        case CONTROLLER_BUTTON_TRIGGER: return (mask >> EVRButtonId_k_EButton_SteamVR_Trigger) & 1;
        case CONTROLLER_BUTTON_GRIP: return (mask >> EVRButtonId_k_EButton_Grip) & 1;
        case CONTROLLER_BUTTON_TOUCHPAD: return (mask >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1;
        default: return 0;
      }
  }
}

static void openvrPoll() {
  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_TrackedDeviceActivated:
        uint32_t id = vrEvent.trackedDeviceIndex;
        if (openvrIsController(id)) {
          Controller* controller = lovrAlloc(Controller, free);
          if (!controller) break;
          controller->id = id;
          vec_push(&state.controllers, controller);
          lovrRetain(controller);
          lovrEventPush((Event) {
            .type = EVENT_CONTROLLER_ADDED,
            .data.controller = { controller }
          });
        }
        break;

      case EVREventType_VREvent_TrackedDeviceDeactivated:
        for (int i = 0; i < state.controllers.length; i++) {
          Controller* controller = state.controllers.data[i];
          if (controller->id == vrEvent.trackedDeviceIndex) {
            lovrRetain(controller);
            lovrEventPush((Event) {
              .type = EVENT_CONTROLLER_REMOVED,
              .data.controller = { controller }
            });
            vec_swapsplice(&state.controllers, i, 1);
            lovrRelease(controller);
            break;
          }
        }
        break;

      case EVREventType_VREvent_ButtonPress:
      case EVREventType_VREvent_ButtonUnpress: {
        bool isPress = vrEvent.eventType == EVREventType_VREvent_ButtonPress;

        if (vrEvent.trackedDeviceIndex == state.headsetIndex && vrEvent.data.controller.button == EVRButtonId_k_EButton_ProximitySensor) {
          lovrEventPush((Event) { .type = EVENT_MOUNT, .data.boolean = { isPress } });
          break;
        }

        for (int i = 0; i < state.controllers.length; i++) {
          Controller* controller = state.controllers.data[i];
          if (controller->id == vrEvent.trackedDeviceIndex) {
            ControllerButton button = getButton(vrEvent.data.controller.button, openvrControllerGetHand(controller));
            lovrEventPush((Event) {
              .type = isPress ? EVENT_CONTROLLER_PRESSED : EVENT_CONTROLLER_RELEASED,
              .data.controller = { controller, button }
            });
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

      default:
        break;
    }
  }
}

static TrackedDevicePose_t getPose(unsigned int deviceIndex) {
  if (state.isRendering) {
    return state.renderPoses[deviceIndex];
  }

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float timeSinceVsync;
  state.system->GetTimeSinceLastVsync(&timeSinceVsync, NULL);
  float frameDuration = 1.f / state.refreshRate;
  float secondsInFuture = frameDuration - timeSinceVsync + state.vsyncToPhotons;
  TrackedDevicePose_t poses[16];
  state.system->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, poses, 16);
  return poses[deviceIndex];
}

static void ensureCanvas() {
  if (state.canvas) {
    return;
  }

  uint32_t width, height;
  state.system->GetRecommendedRenderTargetSize(&width, &height);
  CanvasFlags flags = { .depth = DEPTH_D24S8, .stereo = true, .msaa = state.msaa };
  state.canvas = lovrCanvasCreate(width * 2, height, flags);
  Texture* texture = lovrTextureCreate(TEXTURE_2D, NULL, 0, true, false, state.msaa);
  lovrTextureAllocate(texture, width * 2, height, 1, FORMAT_RGBA);
  lovrCanvasSetAttachments(state.canvas, &(Attachment) { texture, 0, 0 }, 1);
  lovrRelease(texture);
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

  char buffer[128];

  sprintf(buffer, "FnTable:%s", IVRSystem_Version);
  state.system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.system) {
    VR_ShutdownInternal();
    return false;
  }

  sprintf(buffer, "FnTable:%s", IVRCompositor_Version);
  state.compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.compositor) {
    VR_ShutdownInternal();
    return false;
  }

  sprintf(buffer, "FnTable:%s", IVRChaperone_Version);
  state.chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.chaperone) {
    VR_ShutdownInternal();
    return false;
  }

  sprintf(buffer, "FnTable:%s", IVRRenderModels_Version);
  state.renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.renderModels) {
    VR_ShutdownInternal();
    return false;
  }

  state.system->GetStringTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_ManufacturerName_String, buffer, 128, NULL);

  if (!strncmp(buffer, "HTC", 128)) {
    state.type = HEADSET_VIVE;
  } else if (!strncmp(buffer, "Oculus", 128)) {
    state.type = HEADSET_RIFT;
  } else if (!strncmp(buffer, "WindowsMR", 128)) {
    state.type = HEADSET_WINDOWS_MR;
  } else {
    state.type = HEADSET_UNKNOWN;
  }

  state.headsetIndex = k_unTrackedDeviceIndex_Hmd;
  state.refreshRate = state.system->GetFloatTrackedDeviceProperty(state.headsetIndex, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL);
  state.vsyncToPhotons = state.system->GetFloatTrackedDeviceProperty(state.headsetIndex, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, NULL);
  state.isRendering = false;
  state.isMirrored = true;
  state.offset = state.compositor->GetTrackingSpace() == ETrackingUniverseOrigin_TrackingUniverseStanding ? 0. : offset;
  state.msaa = msaa;
  state.canvas = NULL;
  state.clipNear = 0.1f;
  state.clipFar = 30.f;

  for (int i = 0; i < 16; i++) {
    state.deviceModels[i] = NULL;
    state.deviceTextures[i] = NULL;
  }

  vec_init(&state.controllers);
  for (uint32_t i = 0; i < k_unMaxTrackedDeviceCount; i++) {
    if (openvrIsController(i)) {
      Controller* controller = lovrAlloc(Controller, free);
      if (!controller) continue;
      controller->id = i;
      vec_push(&state.controllers, controller);
    }
  }

  lovrEventAddPump(openvrPoll);
  return true;
}

static void openvrDestroy() {
  lovrRelease(state.canvas);
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
    lovrRelease(controller);
  }
  vec_deinit(&state.controllers);
  VR_ShutdownInternal();
  memset(&state, 0, sizeof(HeadsetState));
}

static HeadsetType openvrGetType() {
  return state.type;
}

static HeadsetOrigin openvrGetOriginType() {
  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static bool openvrIsMounted() {
  VRControllerState_t input;
  state.system->GetControllerState(state.headsetIndex, &input, sizeof(input));
  return (input.ulButtonPressed >> EVRButtonId_k_EButton_ProximitySensor) & 1;
}

static bool openvrIsMirrored() {
  return state.isMirrored;
}

static void openvrSetMirrored(bool mirror) {
  state.isMirrored = mirror;
}

static void openvrGetDisplayDimensions(int* width, int* height) {
  ensureCanvas();
  *width = lovrCanvasGetWidth(state.canvas);
  *height = lovrCanvasGetHeight(state.canvas);
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

static void openvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  float transform[16];
  mat4_fromMat44(transform, pose.mDeviceToAbsoluteTracking.m);

  *x = transform[12];
  *y = transform[13];
  *z = transform[14];

  float rotation[4];
  quat_fromMat4(rotation, transform);
  quat_getAngleAxis(rotation, angle, ax, ay, az);
}

static void openvrGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  float transform[16];
  float eyeTransform[16];
  EVREye vrEye = (eye == EYE_LEFT) ? EVREye_Eye_Left : EVREye_Eye_Right;
  mat4_fromMat34(eyeTransform, state.system->GetEyeToHeadTransform(vrEye).m);
  mat4_fromMat44(transform, pose.mDeviceToAbsoluteTracking.m);
  mat4_multiply(transform, eyeTransform);

  *x = transform[12];
  *y = transform[13];
  *z = transform[14];

  float rotation[4];
  quat_fromMat4(rotation, transform);
  quat_getAngleAxis(rotation, angle, ax, ay, az);
}

static void openvrGetVelocity(float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vVelocity.v[0];
  *y = pose.vVelocity.v[1];
  *z = pose.vVelocity.v[2];
}

static void openvrGetAngularVelocity(float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
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

static void openvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  TrackedDevicePose_t pose = getPose(controller->id);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  float transform[16];
  mat4_fromMat44(transform, pose.mDeviceToAbsoluteTracking.m);

  *x = transform[12];
  *y = transform[13];
  *z = transform[14];

  float rotation[4];
  quat_fromMat4(rotation, transform);
  quat_getAngleAxis(rotation, angle, ax, ay, az);
}

static float openvrControllerGetAxis(Controller* controller, ControllerAxis axis) {
  if (!controller) return 0;

  VRControllerState_t input;
  state.system->GetControllerState(controller->id, &input, sizeof(input));

  switch (state.type) {
    case HEADSET_RIFT:
      switch (axis) {
        case CONTROLLER_AXIS_TRIGGER: return input.rAxis[1].x;
        case CONTROLLER_AXIS_GRIP: return input.rAxis[2].x;
        case CONTROLLER_AXIS_TOUCHPAD_X: return input.rAxis[0].x;
        case CONTROLLER_AXIS_TOUCHPAD_Y: return input.rAxis[0].y;
        default: return 0;
      }

    default:
      switch (axis) {
        case CONTROLLER_AXIS_TRIGGER: return input.rAxis[1].x;
        case CONTROLLER_AXIS_TOUCHPAD_X: return input.rAxis[0].x;
        case CONTROLLER_AXIS_TOUCHPAD_Y: return input.rAxis[0].y;
        default: return 0;
      }
  }

  return 0;
}

static bool openvrControllerIsDown(Controller* controller, ControllerButton button) {
  VRControllerState_t input;
  state.system->GetControllerState(controller->id, &input, sizeof(input));
  ControllerHand hand = openvrControllerGetHand(controller);
  return getButtonState(input.ulButtonPressed, button, hand);
}

static bool openvrControllerIsTouched(Controller* controller, ControllerButton button) {
  VRControllerState_t input;
  state.system->GetControllerState(controller->id, &input, sizeof(input));
  ControllerHand hand = openvrControllerGetHand(controller);
  return getButtonState(input.ulButtonTouched, button, hand);
}

static void openvrControllerVibrate(Controller* controller, float duration, float power) {
  if (duration <= 0) return;

  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) (duration * 1e6);
  state.system->TriggerHapticPulse(controller->id, axis, uSeconds);
}

static ModelData* openvrControllerNewModelData(Controller* controller) {
  if (!controller) return NULL;

  int id = controller->id;

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(controller->id, renderModelNameProperty, renderModelName, 1024, NULL);

  // Load model
  if (!state.deviceModels[id]) {
    while (state.renderModels->LoadRenderModel_Async(renderModelName, &state.deviceModels[id]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  // Load texture
  if (!state.deviceTextures[id]) {
    while (state.renderModels->LoadTexture_Async(state.deviceModels[id]->diffuseTextureId, &state.deviceTextures[id]) == EVRRenderModelError_VRRenderModelError_Loading) {
      lovrSleep(.001);
    }
  }

  RenderModel_t* vrModel = state.deviceModels[id];

  ModelData* modelData = lovrModelDataCreateEmpty();
  if (!modelData) return NULL;

  VertexFormat format;
  vertexFormatInit(&format);
  vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);

  modelData->vertexData = lovrVertexDataCreate(vrModel->unVertexCount, &format);

  float* vertices = (float*) modelData->vertexData->blob.data;
  int vertex = 0;
  for (size_t i = 0; i < vrModel->unVertexCount; i++) {
    float* position = vrModel->rVertexData[i].vPosition.v;
    float* normal = vrModel->rVertexData[i].vNormal.v;
    float* texCoords = vrModel->rVertexData[i].rfTextureCoord;

    vertices[vertex++] = position[0];
    vertices[vertex++] = position[1];
    vertices[vertex++] = position[2];

    vertices[vertex++] = normal[0];
    vertices[vertex++] = normal[1];
    vertices[vertex++] = normal[2];

    vertices[vertex++] = texCoords[0];
    vertices[vertex++] = texCoords[1];
  }

  modelData->indexCount = vrModel->unTriangleCount * 3;
  modelData->indexSize = sizeof(uint16_t);
  modelData->indices.raw = malloc(modelData->indexCount * modelData->indexSize);
  memcpy(modelData->indices.raw, vrModel->rIndexData, modelData->indexCount * modelData->indexSize);

  modelData->nodeCount = 1;
  modelData->primitiveCount = 1;
  modelData->animationCount = 0;
  modelData->materialCount = 1;

  modelData->nodes = malloc(1 * sizeof(ModelNode));
  modelData->primitives = malloc(1 * sizeof(ModelPrimitive));
  modelData->animations = NULL;
  modelData->materials = malloc(1 * sizeof(ModelMaterial));

  // Nodes
  ModelNode* root = &modelData->nodes[0];
  root->parent = -1;
  vec_init(&root->children);
  vec_init(&root->primitives);
  vec_push(&root->primitives, 0);
  mat4_identity(root->transform);
  modelData->primitives[0].material = 0;
  modelData->primitives[0].drawStart = 0;
  modelData->primitives[0].drawCount = modelData->indexCount;
  map_init(&modelData->primitives[0].boneMap);
  map_init(&modelData->nodeMap);

  // Material
  RenderModel_TextureMap_t* vrTexture = state.deviceTextures[id];
  TextureData* textureData = lovrTextureDataCreate(vrTexture->unWidth, vrTexture->unHeight, 0, FORMAT_RGBA);
  memcpy(textureData->blob.data, vrTexture->rubTextureMapData, vrTexture->unWidth * vrTexture->unHeight * 4);

  vec_init(&modelData->textures);
  vec_push(&modelData->textures, NULL);
  vec_push(&modelData->textures, textureData);

  ModelMaterial* material = &modelData->materials[0];
  material->diffuseColor = (Color) { 1, 1, 1, 1 };
  material->emissiveColor = (Color) { 0, 0, 0, 1 };
  material->diffuseTexture = 1;
  material->emissiveTexture = 0;
  material->metalnessTexture = 0;
  material->roughnessTexture = 0;
  material->occlusionTexture = 0;
  material->normalTexture = 0;
  material->metalness = 0;
  material->roughness = 0;

  return modelData;
}

static void openvrRenderTo(void (*callback)(void*), void* userdata) {
  float head[16], eye[16];

  ensureCanvas();
  state.isRendering = true;
  state.compositor->WaitGetPoses(state.renderPoses, 16, NULL, 0);
  mat4_fromMat34(head, state.renderPoses[state.headsetIndex].mDeviceToAbsoluteTracking.m);

  Camera camera = { .canvas = state.canvas };

  for (int i = 0; i < 2; i++) {
    EVREye vrEye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;
    mat4_fromMat44(camera.projection[i], state.system->GetProjectionMatrix(vrEye, state.clipNear, state.clipFar).m);
    mat4_identity(camera.viewMatrix[i]);
    mat4_translate(camera.viewMatrix[i], 0, state.offset, 0);
    mat4_multiply(camera.viewMatrix[i], head);
    mat4_multiply(camera.viewMatrix[i], mat4_fromMat34(eye, state.system->GetEyeToHeadTransform(vrEye).m));
    mat4_invert(camera.viewMatrix[i]);
  }

  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
  state.isRendering = false;

  // Submit
  const Attachment* attachments = lovrCanvasGetAttachments(state.canvas, NULL);
  ptrdiff_t id = lovrTextureGetId(attachments[0].texture);
  EColorSpace colorSpace = lovrGraphicsIsGammaCorrect() ? EColorSpace_ColorSpace_Linear : EColorSpace_ColorSpace_Gamma;
  Texture_t eyeTexture = { (void*) id, ETextureType_TextureType_OpenGL, colorSpace };
  VRTextureBounds_t left = { 0, 0, .5, 1. };
  VRTextureBounds_t right = { .5, 0, 1., 1. };
  state.compositor->Submit(EVREye_Eye_Left, &eyeTexture, &left, EVRSubmitFlags_Submit_Default);
  state.compositor->Submit(EVREye_Eye_Right, &eyeTexture, &right, EVRSubmitFlags_Submit_Default);
  lovrGpuDirtyTexture();

  if (state.isMirrored) {
    lovrGraphicsPushPipeline();
    lovrGraphicsSetColor((Color) { 1, 1, 1, 1 });
    lovrGraphicsSetShader(NULL);
    lovrGraphicsFill(attachments[0].texture);
    lovrGraphicsPopPipeline();
  }
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  DRIVER_OPENVR,
  openvrInit,
  openvrDestroy,
  openvrGetType,
  openvrGetOriginType,
  openvrIsMounted,
  openvrIsMirrored,
  openvrSetMirrored,
  openvrGetDisplayDimensions,
  openvrGetClipDistance,
  openvrSetClipDistance,
  openvrGetBoundsDimensions,
  openvrGetPose,
  openvrGetEyePose,
  openvrGetVelocity,
  openvrGetAngularVelocity,
  openvrGetControllers,
  openvrControllerIsConnected,
  openvrControllerGetHand,
  openvrControllerGetPose,
  openvrControllerGetAxis,
  openvrControllerIsDown,
  openvrControllerIsTouched,
  openvrControllerVibrate,
  openvrControllerNewModelData,
  openvrRenderTo,
  NULL
};
