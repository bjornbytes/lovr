#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "util.h"
#include <openvr.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;

  unsigned int headsetIndex;

  int isRendering;
  TrackedDevicePose_t renderPoses[16];
  OpenVRModel deviceModels[16];

  vec_controller_t controllers;

  float clipNear;
  float clipFar;

  uint32_t renderWidth;
  uint32_t renderHeight;

  GLuint framebuffer;
  GLuint depthbuffer;
  GLuint texture;
  GLuint resolveFramebuffer;
  GLuint resolveTexture;
} ViveState;

static ViveState state;

static TrackedDevicePose_t getPose(unsigned int deviceIndex) {
  if (state.isRendering) {
    return state.renderPoses[deviceIndex];
  }

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t poses[16];
  state.system->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, poses, 16);
  return poses[deviceIndex];
}

static Controller* addController(unsigned int deviceIndex) {
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

static void refreshControllers() {
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
      lovrEventPush(event);
      vec_splice(&state.controllers, i, 1);
      lovrRelease(&controller->ref);
    }
  }

  // Add connected controllers that aren't in the list yet
  for (i = 0; i < 2; i++) {
    if ((int) controllerIds[i] != -1) {
      controller = addController(controllerIds[i]);
      if (!controller) continue;
      EventType type = EVENT_CONTROLLER_ADDED;
      EventData data = { .controlleradded = { controller } };
      Event event = { .type = type, .data = data };
      lovrEventPush(event);
    }
  }
}

void lovrHeadsetInit() {
  state.isRendering = 0;
  state.framebuffer = 0;
  state.depthbuffer = 0;
  state.texture = 0;
  state.resolveFramebuffer = 0;
  state.resolveTexture = 0;
  vec_init(&state.controllers);

  for (int i = 0; i < 16; i++) {
    state.deviceModels[i].isLoaded = 0;
    state.deviceModels[i].model = NULL;
    state.deviceModels[i].texture = NULL;
  }

  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    lovrHeadsetDestroy();
    return;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    lovrHeadsetDestroy();
    return;
  }

  char fnTableName[128];

  sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
  state.system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state.system == NULL) {
    lovrHeadsetDestroy();
    return;
  }

  sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
  state.compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state.compositor == NULL) {
    lovrHeadsetDestroy();
    return;
  }

  sprintf(fnTableName, "FnTable:%s", IVRChaperone_Version);
  state.chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state.chaperone == NULL) {
    lovrHeadsetDestroy();
    return;
  }

  sprintf(fnTableName, "FnTable:%s", IVRRenderModels_Version);
  state.renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state.renderModels == NULL) {
    lovrHeadsetDestroy();
    return;
  }

  state.headsetIndex = k_unTrackedDeviceIndex_Hmd;
  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.system->GetRecommendedRenderTargetSize(&state.renderWidth, &state.renderHeight);

  refreshControllers();

  glGenFramebuffers(1, &state.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);

  glGenRenderbuffers(1, &state.depthbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, state.depthbuffer);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, state.renderWidth, state.renderHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, state.depthbuffer);

  glGenTextures(1, &state.texture);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, state.texture);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, state.renderWidth, state.renderHeight, 1);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, state.texture, 0);

  glGenFramebuffers(1, &state.resolveFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, state.resolveFramebuffer);

  glGenTextures(1, &state.resolveTexture);
  glBindTexture(GL_TEXTURE_2D, state.resolveTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, state.renderWidth, state.renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.resolveTexture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    lovrHeadsetDestroy();
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  lovrEventAddPump(lovrHeadsetPoll);
}

void lovrHeadsetPoll() {
  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_TrackedDeviceActivated:
      case EVREventType_VREvent_TrackedDeviceDeactivated:
      case EVREventType_VREvent_TrackedDeviceRoleChanged:
        refreshControllers();
        break;
    }
  }
}

void lovrHeadsetDestroy() {
  glDeleteFramebuffers(1, &state.framebuffer);
  glDeleteFramebuffers(1, &state.resolveFramebuffer);
  glDeleteRenderbuffers(1, &state.depthbuffer);
  glDeleteTextures(1, &state.texture);
  glDeleteTextures(1, &state.resolveTexture);
  for (int i = 0; i < 16; i++) {
    if (state.deviceModels[i].isLoaded) {
      state.renderModels->FreeRenderModel(state.deviceModels[i].model);
    }
  }
  Controller* controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }
  vec_deinit(&state.controllers);
}

char lovrHeadsetIsPresent() {
  return state.system->IsTrackedDeviceConnected(state.headsetIndex);
}

const char* lovrHeadsetGetType() {
  return "Vive";
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  *width = state.renderWidth;
  *height = state.renderHeight;
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  *near = state.clipNear;
  *far = state.clipFar;
}

void lovrHeadsetSetClipDistance(float near, float far) {
  state.clipNear = near;
  state.clipFar = far;
}

float lovrHeadsetGetBoundsWidth() {
  float width;
  state.chaperone->GetPlayAreaSize(&width, NULL);
  return width;
}

float lovrHeadsetGetBoundsDepth() {
  float depth;
  state.chaperone->GetPlayAreaSize(NULL, &depth);
  return depth;
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  struct HmdQuad_t quad;
  state.chaperone->GetPlayAreaRect(&quad);
  for (int i = 0; i < 4; i++) {
    geometry[3 * i + 0] = quad.vCorners[i].v[0];
    geometry[3 * i + 1] = quad.vCorners[i].v[1];
    geometry[3 * i + 2] = quad.vCorners[i].v[2];
  }
}

char lovrHeadsetIsBoundsVisible() {
  return state.chaperone->AreBoundsVisible();
}

void lovrHeadsetSetBoundsVisible(char visible) {
  state.chaperone->ForceBoundsVisible(visible);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float *z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vVelocity.v[0];
  *y = pose.vVelocity.v[1];
  *z = pose.vVelocity.v[2];
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(state.headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
}

vec_controller_t* lovrHeadsetGetControllers() {
  return &state.controllers;
}

char lovrHeadsetControllerIsPresent(Controller* controller) {
  return state.system->IsTrackedDeviceConnected(controller->id);
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(controller->id);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z) {
  TrackedDevicePose_t pose = getPose(controller->id);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  VRControllerState_t input;

  state.system->GetControllerState(controller->id, &input);

  switch (axis) {
    case CONTROLLER_AXIS_TRIGGER:
      return input.rAxis[1].x;

    case CONTROLLER_AXIS_TOUCHPAD_X:
      return input.rAxis[0].x;

    case CONTROLLER_AXIS_TOUCHPAD_Y:
      return input.rAxis[0].y;

    default:
      error("Bad controller axis");
  }

  return 0.;
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  VRControllerState_t input;

  state.system->GetControllerState(controller->id, &input);

  switch (button) {
    case CONTROLLER_BUTTON_SYSTEM:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_ApplicationMenu) & 1;

    case CONTROLLER_BUTTON_MENU:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_System) & 1;

    case CONTROLLER_BUTTON_GRIP:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_Grip) & 1;

    case CONTROLLER_BUTTON_TOUCHPAD:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1;

    default:
      error("Bad controller button");
  }

  return 0;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration) {
  if (duration <= 0) {
    return;
  }

  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) (duration * 1e6);
  state.system->TriggerHapticPulse(controller->id, axis, uSeconds);
}

void* lovrHeadsetControllerGetModel(Controller* controller, ControllerModelFormat* format) {
  *format = CONTROLLER_MODEL_OPENVR;

  // Return the model if it's already loaded
  OpenVRModel* vrModel = &state.deviceModels[controller->id];
  if (vrModel->isLoaded) {
    return vrModel;
  }

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(controller->id, renderModelNameProperty, renderModelName, 1024, NULL);

  // Load model
  RenderModel_t* model = NULL;
  while (state.renderModels->LoadRenderModel_Async(renderModelName, &model) == EVRRenderModelError_VRRenderModelError_Loading) {
    lovrSleep(.001);
  }

  // Load texture
  RenderModel_TextureMap_t* texture = NULL;
  while (model && state.renderModels->LoadTexture_Async(model->diffuseTextureId, &texture) == EVRRenderModelError_VRRenderModelError_Loading) {
    lovrSleep(.001);
  }

  vrModel->isLoaded = 1;
  vrModel->model = model;
  vrModel->texture = texture;

  return vrModel;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  float headMatrix[16], eyeMatrix[16], projectionMatrix[16];
  float (*matrix)[4];
  EGraphicsAPIConvention graphicsConvention = EGraphicsAPIConvention_API_OpenGL;

  state.isRendering = 1;
  state.compositor->WaitGetPoses(state.renderPoses, 16, NULL, 0);
  matrix = state.renderPoses[state.headsetIndex].mDeviceToAbsoluteTracking.m;
  mat4_invert(mat4_fromMat34(headMatrix, matrix));

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    matrix = state.system->GetEyeToHeadTransform(eye).m;
    mat4_invert(mat4_fromMat34(eyeMatrix, matrix));
    mat4 transformMatrix = mat4_multiply(eyeMatrix, headMatrix);

    float near = state.clipNear;
    float far = state.clipFar;
    matrix = state.system->GetProjectionMatrix(eye, near, far, graphicsConvention).m;
    mat4_fromMat44(projectionMatrix, matrix);

    glEnable(GL_MULTISAMPLE);
    glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
    glViewport(0, 0, state.renderWidth, state.renderHeight);

    lovrGraphicsClear(1, 1);
    lovrGraphicsPush();
    lovrGraphicsOrigin();
    lovrGraphicsMatrixTransform(transformMatrix);
    lovrGraphicsSetProjectionRaw(projectionMatrix);
    callback(i, userdata);
    lovrGraphicsPop();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_MULTISAMPLE);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.resolveFramebuffer);
    glBlitFramebuffer(0, 0, state.renderWidth, state.renderHeight, 0, 0, state.renderWidth, state.renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    uintptr_t texture = (uintptr_t) state.resolveTexture;
    Texture_t eyeTexture = { (void*) texture, graphicsConvention, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    state.compositor->Submit(eye, &eyeTexture, NULL, flags);
  }

  state.isRendering = 0;
}
