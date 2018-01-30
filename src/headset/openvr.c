#include "event/event.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/quat.h"
#include "util.h"
#include "graphics/canvas.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <openvr_capi.h>

// From openvr_capi.h
extern intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
extern bool VR_IsHmdPresent();
extern intptr_t VR_GetGenericInterface(const char* pchInterfaceVersion, EVRInitError* peError);
extern bool VR_IsRuntimeInstalled();

static void openvrDestroy();
static void openvrPoll();
static void openvrRefreshControllers();
static ControllerHand openvrControllerGetHand(Controller* controller);
static Controller* openvrAddController(unsigned int deviceIndex);
static ControllerHand openvrControllerGetHand(Controller* controller);

typedef struct {
  bool isInitialized;
  bool isRendering;
  bool isMirrored;

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

  uint32_t renderWidth;
  uint32_t renderHeight;
  float refreshRate;
  float vsyncToPhotons;

  Canvas* canvas;
} HeadsetState;

static HeadsetState state;

static bool openvrIsAvailable() {
  if (VR_IsHmdPresent() && VR_IsRuntimeInstalled()) {
    return true;
  } else {
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

static void initializeCanvas() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  int msaa = 0;
  glGetIntegerv(GL_SAMPLES, &msaa);
  state.system->GetRecommendedRenderTargetSize(&state.renderWidth, &state.renderHeight);
  state.canvas = lovrCanvasCreate(state.renderWidth, state.renderHeight, FORMAT_RGB, CANVAS_3D, msaa, true, true);
}

static void openvrInit() {
  state.isInitialized = false;
  state.isRendering = false;
  state.isMirrored = true;
  state.canvas = NULL;
  vec_init(&state.controllers);

  for (int i = 0; i < 16; i++) {
    state.deviceModels[i] = NULL;
    state.deviceTextures[i] = NULL;
  }

  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    openvrDestroy();
    return;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    openvrDestroy();
    return;
  }

  char buffer[128];

  sprintf(buffer, "FnTable:%s", IVRSystem_Version);
  state.system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.system) {
    openvrDestroy();
    return;
  }

  sprintf(buffer, "FnTable:%s", IVRCompositor_Version);
  state.compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.compositor) {
    openvrDestroy();
    return;
  }

  sprintf(buffer, "FnTable:%s", IVRChaperone_Version);
  state.chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.chaperone) {
    openvrDestroy();
    return;
  }

  sprintf(buffer, "FnTable:%s", IVRRenderModels_Version);
  state.renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  if (vrError != EVRInitError_VRInitError_None || !state.renderModels) {
    openvrDestroy();
    return;
  }

  state.isInitialized = 1;
  state.headsetIndex = k_unTrackedDeviceIndex_Hmd;

  state.system->GetStringTrackedDeviceProperty(state.headsetIndex, ETrackedDeviceProperty_Prop_ManufacturerName_String, buffer, 128, NULL);
  if (!strncmp(buffer, "HTC", 128)) {
    state.type = HEADSET_VIVE;
  } else if (!strncmp(buffer, "Oculus", 128)) {
    state.type = HEADSET_RIFT;
  } else if (!strncmp(buffer, "WindowsMR", 128)) {
    state.type = HEADSET_WINDOWS_MR;
  } else {
    state.type = HEADSET_UNKNOWN;
  }

  state.refreshRate = state.system->GetFloatTrackedDeviceProperty(state.headsetIndex, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL);
  state.vsyncToPhotons = state.system->GetFloatTrackedDeviceProperty(state.headsetIndex, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, NULL);

  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  openvrRefreshControllers();
  lovrEventAddPump(openvrPoll);
}

static void openvrDestroy() {
  state.isInitialized = false;
  if (state.canvas) {
    lovrRelease(&state.canvas->texture.ref);
  }
  for (int i = 0; i < 16; i++) {
    if (state.deviceModels[i]) {
      state.renderModels->FreeRenderModel(state.deviceModels[i]);
    }
    state.deviceModels[i] = NULL;
    state.deviceTextures[i] = NULL;
  }
  Controller* controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }
  vec_deinit(&state.controllers);
}

static void openvrPoll() {
  if (!state.isInitialized) return;
  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_TrackedDeviceActivated:
      case EVREventType_VREvent_TrackedDeviceDeactivated:
      case EVREventType_VREvent_TrackedDeviceRoleChanged: {
        openvrRefreshControllers();
        break;
      }

      case EVREventType_VREvent_ButtonPress:
      case EVREventType_VREvent_ButtonUnpress: {
        bool isPress = vrEvent.eventType == EVREventType_VREvent_ButtonPress;
        Controller* controller;
        int i;
        vec_foreach(&state.controllers, controller, i) {
          if (controller->id == vrEvent.trackedDeviceIndex) {
            ControllerHand hand = openvrControllerGetHand(controller);
            Event event;
            if (isPress) {
              event.type = EVENT_CONTROLLER_PRESSED;
              event.data.controllerpressed.controller = controller;
              event.data.controllerpressed.button = getButton(vrEvent.data.controller.button, hand);
            } else {
              event.type = EVENT_CONTROLLER_RELEASED;
              event.data.controllerreleased.controller = controller;
              event.data.controllerreleased.button = getButton(vrEvent.data.controller.button, hand);
            }
            lovrEventPush(event);
            break;
          }
        }
        break;
      }

      case EVREventType_VREvent_InputFocusCaptured:
      case EVREventType_VREvent_InputFocusReleased: {
        bool isFocused = vrEvent.eventType == EVREventType_VREvent_InputFocusReleased;
        EventData data = { .focus = { isFocused } };
        Event event = { .type = EVENT_FOCUS, .data = data };
        lovrEventPush(event);
        break;
      }
    }
  }
}

static bool openvrIsPresent() {
  return state.isInitialized && state.system->IsTrackedDeviceConnected(state.headsetIndex);
}

static HeadsetType openvrGetType() {
  return state.type;
}

static HeadsetOrigin openvrGetOriginType() {
  if (!state.isInitialized) {
    return ORIGIN_HEAD;
  }

  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static bool openvrIsMirrored() {
  return state.isMirrored;
}

static void openvrSetMirrored(bool mirror) {
  state.isMirrored = mirror;
}

static void openvrGetDisplayDimensions(int* width, int* height) {
  if (!state.isInitialized) {
    *width = *height = 0;
  } else {
    initializeCanvas();
    *width = state.renderWidth;
    *height = state.renderHeight;
  }
}

static void openvrGetClipDistance(float* near, float* far) {
  if (!state.isInitialized) {
    *near = *far = 0.f;
  } else {
    *near = state.clipNear;
    *far = state.clipFar;
  }
}

static void openvrSetClipDistance(float near, float far) {
  if (!state.isInitialized) return;
  state.clipNear = near;
  state.clipFar = far;
}

static float openvrGetBoundsWidth() {
  if (!state.isInitialized) return 0.f;
  float width;
  state.chaperone->GetPlayAreaSize(&width, NULL);
  return width;
}

static float openvrGetBoundsDepth() {
  if (!state.isInitialized) return 0.f;
  float depth;
  state.chaperone->GetPlayAreaSize(NULL, &depth);
  return depth;
}

static void openvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  if (!state.isInitialized) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

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
  if (!state.isInitialized) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

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
  if (!state.isInitialized) {
    *x = *y = *z = 0.f;
    return;
  }

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
  if (!state.isInitialized) {
    *x = *y = *z = 0.f;
    return;
  }

  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
}

static void openvrRefreshControllers() {
  if (!state.isInitialized) return;

  unsigned int leftHand = ETrackedControllerRole_TrackedControllerRole_LeftHand;
  unsigned int leftControllerId = state.system->GetTrackedDeviceIndexForControllerRole(leftHand);

  unsigned int rightHand = ETrackedControllerRole_TrackedControllerRole_RightHand;
  unsigned int rightControllerId = state.system->GetTrackedDeviceIndexForControllerRole(rightHand);

  unsigned int controllerIds[2] = { leftControllerId, rightControllerId };

  // Remove controllers that are no longer recognized as connected
  Controller* controller; int i;
  vec_foreach_rev(&state.controllers, controller, i) {
    if (controller->id != controllerIds[0] && controller->id != controllerIds[1]) {
      EventType type = EVENT_CONTROLLER_REMOVED;
      EventData data = { .controllerremoved = { controller } };
      Event event = { .type = type, .data = data };
      lovrRetain(&controller->ref);
      lovrEventPush(event);
      vec_splice(&state.controllers, i, 1);
      lovrRelease(&controller->ref);
    }
  }

  // Add connected controllers that aren't in the list yet
  for (i = 0; i < 2; i++) {
    if ((int) controllerIds[i] != -1) {
      controller = openvrAddController(controllerIds[i]);
      if (!controller) continue;
      EventType type = EVENT_CONTROLLER_ADDED;
      EventData data = { .controlleradded = { controller } };
      Event event = { .type = type, .data = data };
      lovrRetain(&controller->ref);
      lovrEventPush(event);
    }
  }
}

static Controller* openvrAddController(unsigned int deviceIndex) {
  if (!state.isInitialized) return NULL;

  if ((int) deviceIndex == -1) {
    return NULL;
  }

  Controller* controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    if (controller->id == deviceIndex) {
      return NULL;
    }
  }

  controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
  controller->id = deviceIndex;
  vec_push(&state.controllers, controller);
  return controller;
}

static vec_controller_t* openvrGetControllers() {
  if (!state.isInitialized) return NULL;
  return &state.controllers;
}

static bool openvrControllerIsPresent(Controller* controller) {
  if (!state.isInitialized || !controller) return false;
  return state.system->IsTrackedDeviceConnected(controller->id);
}

static ControllerHand openvrControllerGetHand(Controller* controller) {
  if (!state.isInitialized || !controller) return HAND_UNKNOWN;
  switch (state.system->GetControllerRoleForTrackedDeviceIndex(controller->id)) {
    case ETrackedControllerRole_TrackedControllerRole_LeftHand: return HAND_LEFT;
    case ETrackedControllerRole_TrackedControllerRole_RightHand: return HAND_RIGHT;
    default: return HAND_UNKNOWN;
  }
}

static void openvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  if (!state.isInitialized || !controller) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

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
  if (!state.isInitialized || !controller) return 0.f;

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
  if (!state.isInitialized || !controller) return false;

  VRControllerState_t input;
  state.system->GetControllerState(controller->id, &input, sizeof(input));
  ControllerHand hand = openvrControllerGetHand(controller);
  return getButtonState(input.ulButtonPressed, button, hand);
}

static bool openvrControllerIsTouched(Controller* controller, ControllerButton button) {
  if (!state.isInitialized || !controller) return false;

  VRControllerState_t input;
  state.system->GetControllerState(controller->id, &input, sizeof(input));
  ControllerHand hand = openvrControllerGetHand(controller);
  return getButtonState(input.ulButtonTouched, button, hand);
}

static void openvrControllerVibrate(Controller* controller, float duration, float power) {
  if (!state.isInitialized || !controller || duration <= 0) return;

  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) (duration * 1e6);
  state.system->TriggerHapticPulse(controller->id, axis, uSeconds);
}

static ModelData* openvrControllerNewModelData(Controller* controller) {
  if (!state.isInitialized || !controller) return NULL;

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

  ModelData* modelData = malloc(sizeof(ModelData));
  if (!modelData) return NULL;

  vertexFormatInit(&modelData->format);
  vertexFormatAppend(&modelData->format, "lovrPosition", ATTR_FLOAT, 3);
  vertexFormatAppend(&modelData->format, "lovrNormal", ATTR_FLOAT, 3);
  vertexFormatAppend(&modelData->format, "lovrTexCoord", ATTR_FLOAT, 2);

  modelData->vertexCount = vrModel->unVertexCount;
  modelData->vertices.data = malloc(modelData->vertexCount * modelData->format.stride);

  modelData->indexCount = vrModel->unTriangleCount * 3;
  modelData->indexSize = sizeof(uint16_t);
  modelData->indices.data = malloc(modelData->indexCount * modelData->indexSize);
  memcpy(modelData->indices.data, vrModel->rIndexData, modelData->indexCount * modelData->indexSize);

  float* vertices = modelData->vertices.floats;
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

  modelData->nodeCount = 1;
  modelData->primitiveCount = 1;
  modelData->animationCount = 0;
  modelData->materialCount = 1;

  modelData->nodes = malloc(1 * sizeof(ModelNode));
  modelData->primitives = malloc(1 * sizeof(ModelPrimitive));
  modelData->animations = NULL;
  modelData->materials = malloc(1 * sizeof(ModelMaterial));

  // Geometry
  map_init(&modelData->nodeMap);
  ModelNode* root = &modelData->nodes[0];
  root->parent = -1;
  mat4_identity(root->transform);
  vec_init(&root->children);
  vec_init(&root->primitives);
  vec_push(&root->primitives, 0);
  modelData->primitives[0].material = 0;
  modelData->primitives[0].drawStart = 0;
  modelData->primitives[0].drawCount = modelData->indexCount;
  map_init(&modelData->primitives[0].boneMap);

  // Material
  RenderModel_TextureMap_t* vrTexture = state.deviceTextures[id];

  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  vec_push(&modelData->textures, textureData);

  int width = vrTexture->unWidth;
  int height = vrTexture->unHeight;
  size_t size = width * height * 4;

  textureData->width = width;
  textureData->height = height;
  textureData->format = FORMAT_RGBA;
  textureData->data = memcpy(malloc(size), vrTexture->rubTextureMapData, size);
  textureData->blob = NULL;
  textureData->generateMipmaps = true;
  vec_init(&textureData->mipmaps);

  modelData->materials[0].diffuseColor = (Color) { 1, 1, 1, 1 };
  modelData->materials[0].diffuseTexture = 0;

  return modelData;
}

static void openvrRenderTo(headsetRenderCallback callback, void* userdata) {
  if (!state.isInitialized) return;

  lovrGraphicsPushView();

  if (!state.canvas) {
    initializeCanvas();
  }

  float head[16], transform[16], projection[16];
  float (*matrix)[4];

  state.isRendering = true;
  state.compositor->WaitGetPoses(state.renderPoses, 16, NULL, 0);

  // Head transform
  matrix = state.renderPoses[state.headsetIndex].mDeviceToAbsoluteTracking.m;
  mat4_invert(mat4_fromMat34(head, matrix));

  for (HeadsetEye eye = EYE_LEFT; eye <= EYE_RIGHT; eye++) {

    // Eye transform
    EVREye vrEye = (eye == EYE_LEFT) ? EVREye_Eye_Left : EVREye_Eye_Right;
    matrix = state.system->GetEyeToHeadTransform(vrEye).m;
    mat4_invert(mat4_fromMat34(transform, matrix));
    mat4_multiply(transform, head);

    // Projection
    matrix = state.system->GetProjectionMatrix(vrEye, state.clipNear, state.clipFar).m;
    mat4_fromMat44(projection, matrix);

    // Render
    lovrCanvasBind(state.canvas);
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_VIEW, transform);
    lovrGraphicsSetProjection(projection);
    lovrGraphicsClear(true, true, false);
    callback(eye, userdata);
    lovrGraphicsPop();
    lovrCanvasResolveMSAA(state.canvas);

    // OpenVR changes the OpenGL texture binding, so we reset it after rendering
    glActiveTexture(GL_TEXTURE0);
    Texture* oldTexture = lovrGraphicsGetTexture(0);

    // Submit
    uintptr_t texture = (uintptr_t) state.canvas->texture.id;
    ETextureType textureType = ETextureType_TextureType_OpenGL;
    EColorSpace colorSpace = lovrGraphicsIsGammaCorrect() ? EColorSpace_ColorSpace_Linear : EColorSpace_ColorSpace_Gamma;
    Texture_t eyeTexture = { (void*) texture, textureType, colorSpace };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    state.compositor->Submit(vrEye, &eyeTexture, NULL, flags);

    // Reset to the correct texture
    glBindTexture(GL_TEXTURE_2D, oldTexture->id);
  }

  state.isRendering = false;
  lovrGraphicsPopView();

  if (state.isMirrored) {
    Color oldColor = lovrGraphicsGetColor();
    lovrGraphicsSetColor((Color) { 1, 1, 1, 1 });
    Shader* lastShader = lovrGraphicsGetShader();

    if (lastShader) {
      lovrRetain(&lastShader->ref);
    }

    lovrGraphicsSetShader(NULL);
    lovrGraphicsPlaneFullscreen(&state.canvas->texture);
    lovrGraphicsSetShader(lastShader);

    if (lastShader) {
      lovrRelease(&lastShader->ref);
    }

    lovrGraphicsSetColor(oldColor);
  }
}

static void openvrUpdate(float dt) {
  //
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  DRIVER_OPENVR,
  openvrIsAvailable,
  openvrInit,
  openvrDestroy,
  openvrPoll,
  openvrIsPresent,
  openvrGetType,
  openvrGetOriginType,
  openvrIsMirrored,
  openvrSetMirrored,
  openvrGetDisplayDimensions,
  openvrGetClipDistance,
  openvrSetClipDistance,
  openvrGetBoundsWidth,
  openvrGetBoundsDepth,
  openvrGetPose,
  openvrGetEyePose,
  openvrGetVelocity,
  openvrGetAngularVelocity,
  openvrGetControllers,
  openvrControllerIsPresent,
  openvrControllerGetHand,
  openvrControllerGetPose,
  openvrControllerGetAxis,
  openvrControllerIsDown,
  openvrControllerIsTouched,
  openvrControllerVibrate,
  openvrControllerNewModelData,
  openvrRenderTo,
  openvrUpdate
};
