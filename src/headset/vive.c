#include "headset/vive.h"
#include "graphics/graphics.h"
#include "util.h"
#include <stdlib.h>
#include <stdint.h>

static TrackedDevicePose_t viveGetPose(Vive* vive, unsigned int deviceIndex) {
  if (vive->isRendering) {
    return vive->renderPoses[deviceIndex];
  }

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t poses[16];
  vive->system->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, poses, 16);
  return poses[deviceIndex];
}

Headset* viveInit() {
  Vive* vive = malloc(sizeof(Vive));
  if (!vive) return NULL;

  Headset* headset = (Headset*) vive;
  headset->isPresent = viveIsPresent;
  headset->getType = viveGetType;
  headset->getDisplayDimensions = viveGetDisplayDimensions;
  headset->getClipDistance = viveGetClipDistance;
  headset->setClipDistance = viveSetClipDistance;
  headset->getBoundsWidth = viveGetBoundsWidth;
  headset->getBoundsDepth = viveGetBoundsDepth;
  headset->getBoundsGeometry = viveGetBoundsGeometry;
  headset->isBoundsVisible = viveIsBoundsVisible;
  headset->setBoundsVisible = viveSetBoundsVisible;
  headset->getPosition = viveGetPosition;
  headset->getOrientation = viveGetOrientation;
  headset->getVelocity = viveGetVelocity;
  headset->getAngularVelocity = viveGetAngularVelocity;
  headset->getController = viveGetController;
  headset->controllerIsPresent = viveControllerIsPresent;
  headset->controllerGetPosition = viveControllerGetPosition;
  headset->controllerGetOrientation = viveControllerGetOrientation;
  headset->controllerGetAxis = viveControllerGetAxis;
  headset->controllerIsDown = viveControllerIsDown;
  headset->controllerGetHand = viveControllerGetHand;
  headset->controllerVibrate = viveControllerVibrate;
  headset->controllerGetModel = viveControllerGetModel;
  headset->renderTo = viveRenderTo;

  vive->isRendering = 0;
  vive->controllers[CONTROLLER_HAND_LEFT] = NULL;
  vive->controllers[CONTROLLER_HAND_RIGHT] = NULL;
  vive->framebuffer = 0;
  vive->depthbuffer = 0;
  vive->texture = 0;
  vive->resolveFramebuffer = 0;
  vive->resolveTexture = 0;

  for (int i = 0; i < 16; i++) {
    vive->deviceModels[i].isLoaded = 0;
    vive->deviceModels[i].model = NULL;
    vive->deviceModels[i].texture = NULL;
  }

  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    viveDestroy(vive);
    return NULL;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    viveDestroy(vive);
    return NULL;
  }

  char fnTableName[128];

  sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
  vive->system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || vive->system == NULL) {
    viveDestroy(vive);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
  vive->compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || vive->compositor == NULL) {
    viveDestroy(vive);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRChaperone_Version);
  vive->chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || vive->chaperone == NULL) {
    viveDestroy(vive);
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRRenderModels_Version);
  vive->renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || vive->renderModels == NULL) {
    viveDestroy(vive);
    return NULL;
  }

  vive->headsetIndex = k_unTrackedDeviceIndex_Hmd;
  vive->clipNear = 0.1f;
  vive->clipFar = 30.f;
  vive->system->GetRecommendedRenderTargetSize(&vive->renderWidth, &vive->renderHeight);

  Controller* leftController = lovrAlloc(sizeof(Controller), NULL);
  if (!leftController) {
    viveDestroy(vive);
    return NULL;
  }
  leftController->hand = CONTROLLER_HAND_LEFT;
  vive->controllers[CONTROLLER_HAND_LEFT] = leftController;
  vive->controllerIndex[CONTROLLER_HAND_LEFT] = vive->system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);

  Controller* rightController = lovrAlloc(sizeof(Controller), NULL);
  if (!rightController) {
    viveDestroy(vive);
    return NULL;
  }
  rightController->hand = CONTROLLER_HAND_RIGHT;
  vive->controllers[CONTROLLER_HAND_RIGHT] = rightController;
  vive->controllerIndex[CONTROLLER_HAND_RIGHT] = vive->system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);

  glGenFramebuffers(1, &vive->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, vive->framebuffer);

  glGenRenderbuffers(1, &vive->depthbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, vive->depthbuffer);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, vive->renderWidth, vive->renderHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, vive->depthbuffer);

  glGenTextures(1, &vive->texture);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, vive->texture);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, vive->renderWidth, vive->renderHeight, 1);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, vive->texture, 0);

  glGenFramebuffers(1, &vive->resolveFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, vive->resolveFramebuffer);

  glGenTextures(1, &vive->resolveTexture);
  glBindTexture(GL_TEXTURE_2D, vive->resolveTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vive->renderWidth, vive->renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vive->resolveTexture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    viveDestroy(vive);
    return NULL;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return headset;
}

void viveDestroy(void* headset) {
  Vive* vive = (Vive*) headset;
  glDeleteFramebuffers(1, &vive->framebuffer);
  glDeleteFramebuffers(1, &vive->resolveFramebuffer);
  glDeleteRenderbuffers(1, &vive->depthbuffer);
  glDeleteTextures(1, &vive->texture);
  glDeleteTextures(1, &vive->resolveTexture);
  for (int i = 0; i < 16; i++) {
    if (vive->deviceModels[i].isLoaded) {
      vive->renderModels->FreeRenderModel(vive->deviceModels[i].model);
    }
  }
  free(vive->controllers[CONTROLLER_HAND_LEFT]);
  free(vive->controllers[CONTROLLER_HAND_RIGHT]);
  free(vive);
}

char viveIsPresent(void* headset) {
  Vive* vive = (Vive*) headset;
  return vive->system->IsTrackedDeviceConnected(vive->headsetIndex);
}

const char* viveGetType(void* headset) {
  return "Vive";
}

void viveGetDisplayDimensions(void* headset, int* width, int* height) {
  Vive* vive = (Vive*) headset;
  *width = vive->renderWidth;
  *height = vive->renderHeight;
}

void viveGetClipDistance(void* headset, float* near, float* far) {
  Vive* vive = (Vive*) headset;
  *near = vive->clipNear;
  *far = vive->clipFar;
}

void viveSetClipDistance(void* headset, float near, float far) {
  Vive* vive = (Vive*) headset;
  vive->clipNear = near;
  vive->clipFar = far;
}

float viveGetBoundsWidth(void* headset) {
  Vive* vive = (Vive*) headset;
  float width;
  vive->chaperone->GetPlayAreaSize(&width, NULL);
  return width;
}

float viveGetBoundsDepth(void* headset) {
  Vive* vive = (Vive*) headset;
  float depth;
  vive->chaperone->GetPlayAreaSize(NULL, &depth);
  return depth;
}

void viveGetBoundsGeometry(void* headset, float* geometry) {
  Vive* vive = (Vive*) headset;
  struct HmdQuad_t quad;
  vive->chaperone->GetPlayAreaRect(&quad);
  for (int i = 0; i < 4; i++) {
    geometry[3 * i + 0] = quad.vCorners[i].v[0];
    geometry[3 * i + 1] = quad.vCorners[i].v[1];
    geometry[3 * i + 2] = quad.vCorners[i].v[2];
  }
}

char viveIsBoundsVisible(void* headset) {
  Vive* vive = (Vive*) headset;
  return vive->chaperone->AreBoundsVisible();
}

void viveSetBoundsVisible(void* headset, char visible) {
  Vive* vive = (Vive*) headset;
  vive->chaperone->ForceBoundsVisible(visible);
}

void viveGetPosition(void* headset, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void viveGetOrientation(void* headset, float* w, float* x, float* y, float *z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

void viveGetVelocity(void* headset, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vVelocity.v[0];
  *y = pose.vVelocity.v[1];
  *z = pose.vVelocity.v[2];
}

void viveGetAngularVelocity(void* headset, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->headsetIndex);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
}

Controller* viveGetController(void* headset, ControllerHand hand) {
  Vive* vive = (Vive*) headset;
  return vive->controllers[hand];
}

char viveControllerIsPresent(void* headset, Controller* controller) {
  Vive* vive = (Vive*) headset;
  return vive->system->IsTrackedDeviceConnected(vive->controllerIndex[controller->hand]);
}

void viveControllerGetPosition(void* headset, Controller* controller, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->controllerIndex[controller->hand]);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

void viveControllerGetOrientation(void* headset, Controller* controller, float* w, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, vive->controllerIndex[controller->hand]);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  float matrix[16];
  mat4_getRotation(mat4_fromMat44(matrix, pose.mDeviceToAbsoluteTracking.m), w, x, y, z);
}

float viveControllerGetAxis(void* headset, Controller* controller, ControllerAxis axis) {
  Vive* vive = (Vive*) headset;
  VRControllerState_t input;

  vive->system->GetControllerState(vive->controllerIndex[controller->hand], &input);

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
  Vive* vive = (Vive*) headset;
  VRControllerState_t input;

  vive->system->GetControllerState(vive->controllerIndex[controller->hand], &input);

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

  Vive* vive = (Vive*) headset;
  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) duration * 1e6;
  vive->system->TriggerHapticPulse(vive->controllerIndex[controller->hand], axis, uSeconds);
}

void* viveControllerGetModel(void* headset, Controller* controller, ControllerModelFormat* format) {
  Vive* vive = (Vive*) headset;

  *format = CONTROLLER_MODEL_OPENVR;

  // Return the model if it's already loaded
  unsigned int deviceIndex = vive->controllerIndex[controller->hand];
  OpenVRModel* vrModel = &vive->deviceModels[deviceIndex];
  if (vrModel->isLoaded) {
    return vrModel;
  }

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  vive->system->GetStringTrackedDeviceProperty(deviceIndex, renderModelNameProperty, renderModelName, 1024, NULL);

  // Load model
  RenderModel_t* model = NULL;
  while (vive->renderModels->LoadRenderModel_Async(renderModelName, &model) == EVRRenderModelError_VRRenderModelError_Loading) {
    lovrSleep(.001);
  }

  // Load texture
  RenderModel_TextureMap_t* texture = NULL;
  while (model && vive->renderModels->LoadTexture_Async(model->diffuseTextureId, &texture) == EVRRenderModelError_VRRenderModelError_Loading) {
    lovrSleep(.001);
  }

  vrModel->isLoaded = 1;
  vrModel->model = model;
  vrModel->texture = texture;

  return vrModel;
}

void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata) {
  Vive* vive = (Vive*) headset;
  float headMatrix[16], eyeMatrix[16], projectionMatrix[16];
  float (*matrix)[4];
  EGraphicsAPIConvention graphicsConvention = EGraphicsAPIConvention_API_OpenGL;

  vive->isRendering = 1;
  vive->compositor->WaitGetPoses(vive->renderPoses, 16, NULL, 0);
  matrix = vive->renderPoses[vive->headsetIndex].mDeviceToAbsoluteTracking.m;
  mat4_invert(mat4_fromMat34(headMatrix, matrix));

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    matrix = vive->system->GetEyeToHeadTransform(eye).m;
    mat4_invert(mat4_fromMat34(eyeMatrix, matrix));
    mat4 transformMatrix = mat4_multiply(eyeMatrix, headMatrix);

    float near = vive->clipNear;
    float far = vive->clipFar;
    matrix = vive->system->GetProjectionMatrix(eye, near, far, graphicsConvention).m;
    mat4_fromMat44(projectionMatrix, matrix);

    glEnable(GL_MULTISAMPLE);
    glBindFramebuffer(GL_FRAMEBUFFER, vive->framebuffer);
    glViewport(0, 0, vive->renderWidth, vive->renderHeight);

    lovrGraphicsClear(1, 1);
    lovrGraphicsPush();
    lovrGraphicsOrigin();
    lovrGraphicsMatrixTransform(transformMatrix);
    lovrGraphicsSetProjectionRaw(projectionMatrix);
    callback(i, userdata);
    lovrGraphicsPop();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_MULTISAMPLE);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, vive->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vive->resolveFramebuffer);
    glBlitFramebuffer(0, 0, vive->renderWidth, vive->renderHeight, 0, 0, vive->renderWidth, vive->renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    uintptr_t texture = (uintptr_t) vive->resolveTexture;
    Texture_t eyeTexture = { (void*) texture, graphicsConvention, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    vive->compositor->Submit(eye, &eyeTexture, NULL, flags);
  }

  vive->isRendering = 0;
}
