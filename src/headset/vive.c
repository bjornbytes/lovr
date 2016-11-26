#include "headset/vive.h"
#include "graphics/graphics.h"
#include "util.h"
#include <stdlib.h>
#include <stdint.h>

static HeadsetInterface interface = {
  .isPresent = viveIsPresent,
  .getType = viveGetType,
  .getDisplayDimensions = viveGetDisplayDimensions,
  .getClipDistance = viveGetClipDistance,
  .setClipDistance = viveSetClipDistance,
  .getBoundsWidth = viveGetBoundsWidth,
  .getBoundsDepth = viveGetBoundsDepth,
  .getBoundsGeometry = viveGetBoundsGeometry,
  .isBoundsVisible = viveIsBoundsVisible,
  .setBoundsVisible = viveSetBoundsVisible,
  .getPosition = viveGetPosition,
  .getOrientation = viveGetOrientation,
  .getVelocity = viveGetVelocity,
  .getAngularVelocity = viveGetAngularVelocity,
  .getController = viveGetController,
  .controllerIsPresent = viveControllerIsPresent,
  .controllerGetPosition = viveControllerGetPosition,
  .controllerGetOrientation = viveControllerGetOrientation,
  .controllerGetAxis = viveControllerGetAxis,
  .controllerIsDown = viveControllerIsDown,
  .controllerGetHand = viveControllerGetHand,
  .controllerVibrate = viveControllerVibrate,
  .controllerGetModel = viveControllerGetModel,
  .renderTo = viveRenderTo
};

static TrackedDevicePose_t viveGetPose(ViveState* state, unsigned int deviceIndex) {
  if (state->isRendering) {
    return state->renderPoses[deviceIndex];
  }

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t poses[16];
  state->vrSystem->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, poses, 16);
  return poses[deviceIndex];
}

Headset* viveInit() {
  Headset* this = malloc(sizeof(Headset));
  if (!this) return NULL;

  ViveState* state = malloc(sizeof(ViveState));
  if (!state) return NULL;

  this->state = state;
  this->interface = &interface;
  state->isRendering = 0;
  state->controllers[CONTROLLER_HAND_LEFT] = NULL;
  state->controllers[CONTROLLER_HAND_RIGHT] = NULL;
  state->framebuffer = 0;
  state->depthbuffer = 0;
  state->texture = 0;
  state->resolveFramebuffer = 0;
  state->resolveTexture = 0;

  for (int i = 0; i < 16; i++) {
    state->renderModels[i] = NULL;
  }

  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    viveDestroy(this);
    return NULL;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    viveDestroy(this);
    return NULL;
  }

  char fnTableName[128];

  sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
  state->vrSystem = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrSystem == NULL) {
    viveDestroy(this);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
  state->vrCompositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrCompositor == NULL) {
    viveDestroy(this);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRChaperone_Version);
  state->vrChaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrChaperone == NULL) {
    viveDestroy(this);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRRenderModels_Version);
  state->vrRenderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrRenderModels == NULL) {
    viveDestroy(this);
    return NULL;
  }

  state->headsetIndex = k_unTrackedDeviceIndex_Hmd;
  state->clipNear = 0.1f;
  state->clipFar = 30.f;
  state->vrSystem->GetRecommendedRenderTargetSize(&state->renderWidth, &state->renderHeight);

  Controller* leftController = lovrAlloc(sizeof(Controller), NULL);
  if (!leftController) {
    viveDestroy(this);
    return NULL;
  }
  leftController->hand = CONTROLLER_HAND_LEFT;
  state->controllers[CONTROLLER_HAND_LEFT] = leftController;
  state->controllerIndex[CONTROLLER_HAND_LEFT] = state->vrSystem->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);

  Controller* rightController = lovrAlloc(sizeof(Controller), NULL);
  if (!rightController) {
    viveDestroy(this);
    return NULL;
  }
  rightController->hand = CONTROLLER_HAND_RIGHT;
  state->controllers[CONTROLLER_HAND_RIGHT] = rightController;
  state->controllerIndex[CONTROLLER_HAND_RIGHT] = state->vrSystem->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);

  glGenFramebuffers(1, &state->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);

  glGenRenderbuffers(1, &state->depthbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, state->depthbuffer);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, state->renderWidth, state->renderHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, state->depthbuffer);

  glGenTextures(1, &state->texture);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, state->texture);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, state->renderWidth, state->renderHeight, 1);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, state->texture, 0);

  glGenFramebuffers(1, &state->resolveFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, state->resolveFramebuffer);

  glGenTextures(1, &state->resolveTexture);
  glBindTexture(GL_TEXTURE_2D, state->resolveTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, state->renderWidth, state->renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->resolveTexture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    viveDestroy(this);
    return NULL;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return this;
}

void viveDestroy(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  glDeleteFramebuffers(1, &state->framebuffer);
  glDeleteFramebuffers(1, &state->resolveFramebuffer);
  glDeleteRenderbuffers(1, &state->depthbuffer);
  glDeleteTextures(1, &state->texture);
  glDeleteTextures(1, &state->resolveTexture);
  for (int i = 0; i < 16; i++) {
    if (state->renderModels[i]) {
      state->vrRenderModels->FreeRenderModel(state->renderModels[i]);
    }
  }
  free(state->controllers[CONTROLLER_HAND_LEFT]);
  free(state->controllers[CONTROLLER_HAND_RIGHT]);
  free(state);
  free(this);
}

char viveIsPresent(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  return state->vrSystem->IsTrackedDeviceConnected(state->headsetIndex);
}

const char* viveGetType(void* headset) {
  return "Vive";
}

void viveGetDisplayDimensions(void* headset, int* width, int* height) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  *width = state->renderWidth;
  *height = state->renderHeight;
}

void viveGetClipDistance(void* headset, float* near, float* far) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  *near = state->clipNear;
  *far = state->clipFar;
}

void viveSetClipDistance(void* headset, float near, float far) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  state->clipNear = near;
  state->clipFar = far;
}

float viveGetBoundsWidth(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  float width;
  state->vrChaperone->GetPlayAreaSize(&width, NULL);
  return width;
}

float viveGetBoundsDepth(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  float depth;
  state->vrChaperone->GetPlayAreaSize(NULL, &depth);
  return depth;
}

void viveGetBoundsGeometry(void* headset, float* geometry) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  struct HmdQuad_t quad;
  state->vrChaperone->GetPlayAreaRect(&quad);
  for (int i = 0; i < 4; i++) {
    geometry[3 * i + 0] = quad.vCorners[i].v[0];
    geometry[3 * i + 1] = quad.vCorners[i].v[1];
    geometry[3 * i + 2] = quad.vCorners[i].v[2];
  }
}

char viveIsBoundsVisible(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  return state->vrChaperone->AreBoundsVisible();
}

void viveSetBoundsVisible(void* headset, char visible) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  state->vrChaperone->ForceBoundsVisible(visible);
}

void viveGetPosition(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void viveGetOrientation(void* headset, float* w, float* x, float* y, float *z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

void viveGetVelocity(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vVelocity.v[0];
  *y = pose.vVelocity.v[1];
  *z = pose.vVelocity.v[2];
}

void viveGetAngularVelocity(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
}

Controller* viveGetController(void* headset, ControllerHand hand) {
  Headset* this = headset;
  ViveState* state = this->state;
  return state->controllers[hand];
}

char viveControllerIsPresent(void* headset, Controller* controller) {
  Headset* this = headset;
  ViveState* state = this->state;
  return state->vrSystem->IsTrackedDeviceConnected(state->controllerIndex[controller->hand]);
}

void viveControllerGetPosition(void* headset, Controller* controller, float* x, float* y, float* z) {
  Headset* this = headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->controllerIndex[controller->hand]);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void viveControllerGetOrientation(void* headset, Controller* controller, float* w, float* x, float* y, float* z) {
  Headset* this = headset;
  ViveState* state = this->state;
  TrackedDevicePose_t pose = viveGetPose(state, state->controllerIndex[controller->hand]);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

float viveControllerGetAxis(void* headset, Controller* controller, ControllerAxis axis) {
  Headset* this = headset;
  ViveState* state = this->state;
  VRControllerState_t input;

  state->vrSystem->GetControllerState(state->controllerIndex[controller->hand], &input);

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

int viveControllerIsDown(void* headset, Controller* controller, ControllerButton button) {
  Headset* this = headset;
  ViveState* state = this->state;
  VRControllerState_t input;

  state->vrSystem->GetControllerState(state->controllerIndex[controller->hand], &input);

  switch (button) {
    case CONTROLLER_BUTTON_SYSTEM:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_System) & 1;

    case CONTROLLER_BUTTON_MENU:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_ApplicationMenu) & 1;

    case CONTROLLER_BUTTON_GRIP:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_Grip) & 1;

    case CONTROLLER_BUTTON_TOUCHPAD:
      return (input.ulButtonPressed >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1;

    default:
      error("Bad controller button");
  }

  return 0;
}

ControllerHand viveControllerGetHand(void* headset, Controller* controller) {
  return controller->hand;
}

void viveControllerVibrate(void* headset, Controller* controller, float duration) {
  if (duration <= 0) {
    return;
  }

  Headset* this = headset;
  ViveState* state = this->state;
  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) duration * 1e6;
  state->vrSystem->TriggerHapticPulse(state->controllerIndex[controller->hand], axis, uSeconds);
}

void* viveControllerGetModel(void* headset, Controller* controller, ControllerModelFormat* format) {
  Headset* this = headset;
  ViveState* state = this->state;

  *format = CONTROLLER_MODEL_OPENVR;

  // Return the model if it's already loaded
  unsigned int deviceIndex = state->controllerIndex[controller->hand];
  if (state->renderModels[deviceIndex]) {
    return state->renderModels[deviceIndex];
  }

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state->vrSystem->GetStringTrackedDeviceProperty(deviceIndex, renderModelNameProperty, renderModelName, 1024, NULL);

  // Load model
  RenderModel_t* renderModel = NULL;
  while (state->vrRenderModels->LoadRenderModel_Async(renderModelName, &renderModel) == EVRRenderModelError_VRRenderModelError_Loading) {
    lovrSleep(.001);
  }

  state->renderModels[deviceIndex] = renderModel;
  return renderModel;
}

void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata) {
  Headset* this = headset;
  ViveState* state = this->state;
  float headMatrix[16], eyeMatrix[16], projectionMatrix[16];
  float (*matrix)[4];
  EGraphicsAPIConvention graphicsConvention = EGraphicsAPIConvention_API_OpenGL;

  state->isRendering = 1;
  state->vrCompositor->WaitGetPoses(state->renderPoses, 16, NULL, 0);
  matrix = state->renderPoses[state->headsetIndex].mDeviceToAbsoluteTracking.m;
  mat4_invert(mat4_fromMat34(headMatrix, matrix));

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    matrix = state->vrSystem->GetEyeToHeadTransform(eye).m;
    mat4_invert(mat4_fromMat34(eyeMatrix, matrix));
    mat4 transformMatrix = mat4_multiply(eyeMatrix, headMatrix);

    float near = state->clipNear;
    float far = state->clipFar;
    matrix = state->vrSystem->GetProjectionMatrix(eye, near, far, graphicsConvention).m;
    mat4_fromMat44(projectionMatrix, matrix);

    glEnable(GL_MULTISAMPLE);
    glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);
    glViewport(0, 0, state->renderWidth, state->renderHeight);

    lovrGraphicsClear(1, 1);
    lovrGraphicsPush();
    lovrGraphicsOrigin();
    lovrGraphicsMatrixTransform(transformMatrix);
    lovrGraphicsSetProjectionRaw(projectionMatrix);
    callback(i, userdata);
    lovrGraphicsPop();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_MULTISAMPLE);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, state->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state->resolveFramebuffer);
    glBlitFramebuffer(0, 0, state->renderWidth, state->renderHeight, 0, 0, state->renderWidth, state->renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    uintptr_t texture = (uintptr_t) state->resolveTexture;
    Texture_t eyeTexture = { (void*) texture, graphicsConvention, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    state->vrCompositor->Submit(eye, &eyeTexture, NULL, flags);
  }

  state->isRendering = 0;
}
