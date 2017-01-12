#include "headset/vive.h"
#include "event/event.h"
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

static void viveRefreshControllers(Vive* vive) {
  unsigned int leftHand = ETrackedControllerRole_TrackedControllerRole_LeftHand;
  unsigned int leftControllerId = vive->system->GetTrackedDeviceIndexForControllerRole(leftHand);

  unsigned int rightHand = ETrackedControllerRole_TrackedControllerRole_RightHand;
  unsigned int rightControllerId = vive->system->GetTrackedDeviceIndexForControllerRole(rightHand);

  unsigned int controllerIds[2] = { leftControllerId, rightControllerId };

  // Remove controllers that are no longer recognized as connected
  Controller* controller; int i;
  vec_foreach_rev(&vive->controllers, controller, i) {
    if (controller->id != controllerIds[0] && controller->id != controllerIds[1]) {
      EventType type = EVENT_CONTROLLER_REMOVED;
      EventData data = { .controllerremoved = { controller } };
      Event event = { .type = type, .data = data };
      lovrEventPush(event);
      vec_splice(&vive->controllers, i, 1);
      lovrRelease(&controller->ref);
    }
  }

  // Add connected controllers that aren't in the list yet
  for (i = 0; i < 2; i++) {
    if ((int) controllerIds[i] != -1) {
      controller = viveAddController((void*) vive, controllerIds[i]);
      if (!controller) continue;
      EventType type = EVENT_CONTROLLER_ADDED;
      EventData data = { .controlleradded = { controller } };
      Event event = { .type = type, .data = data };
      lovrEventPush(event);
    }
  }
}

Headset* viveInit() {
  Vive* vive = malloc(sizeof(Vive));
  if (!vive) return NULL;

  Headset* headset = (Headset*) vive;
  headset->poll = vivePoll;
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
  headset->getControllers = viveGetControllers;
  headset->controllerIsPresent = viveControllerIsPresent;
  headset->controllerGetPosition = viveControllerGetPosition;
  headset->controllerGetOrientation = viveControllerGetOrientation;
  headset->controllerGetAxis = viveControllerGetAxis;
  headset->controllerIsDown = viveControllerIsDown;
  headset->controllerVibrate = viveControllerVibrate;
  headset->controllerGetModel = viveControllerGetModel;
  headset->renderTo = viveRenderTo;

  vive->isRendering = 0;
  vive->framebuffer = 0;
  vive->depthbuffer = 0;
  vive->texture = 0;
  vive->resolveFramebuffer = 0;
  vive->resolveTexture = 0;
  vec_init(&vive->controllers);

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

  viveRefreshControllers(vive);

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

void vivePoll(void* headset) {
  Vive* vive = (Vive*) headset;

  struct VREvent_t vrEvent;
  while (vive->system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_TrackedDeviceActivated:
      case EVREventType_VREvent_TrackedDeviceDeactivated:
      case EVREventType_VREvent_TrackedDeviceRoleChanged:
        viveRefreshControllers(vive);
        break;
    }
  }
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
  Controller* controller; int i;
  vec_foreach(&vive->controllers, controller, i) {
    lovrRelease(&controller->ref);
  }
  vec_deinit(&vive->controllers);
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

Controller* viveAddController(void* headset, unsigned int deviceIndex) {
  if ((int) deviceIndex == -1) {
    return NULL;
  }

  Vive* vive = (Vive*) headset;
  Controller* controller; int i;
  vec_foreach(&vive->controllers, controller, i) {
    if (controller->id == deviceIndex) {
      return NULL;
    }
  }

  controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
  controller->id = deviceIndex;
  vec_push(&vive->controllers, controller);
  return controller;
}

vec_controller_t* viveGetControllers(void* headset) {
  Vive* vive = (Vive*) headset;
  return &vive->controllers;
}

char viveControllerIsPresent(void* headset, Controller* controller) {
  Vive* vive = (Vive*) headset;
  return vive->system->IsTrackedDeviceConnected(controller->id);
}

void viveControllerGetPosition(void* headset, Controller* controller, float* x, float* y, float* z) {
  Vive* vive = (Vive*) headset;
  TrackedDevicePose_t pose = viveGetPose(vive, controller->id);

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
  TrackedDevicePose_t pose = viveGetPose(vive, controller->id);

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

  vive->system->GetControllerState(controller->id, &input, sizeof(input));

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

  vive->system->GetControllerState(controller->id, &input, sizeof(input));

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

void viveControllerVibrate(void* headset, Controller* controller, float duration) {
  if (duration <= 0) {
    return;
  }

  Vive* vive = (Vive*) headset;
  uint32_t axis = 0;
  unsigned short uSeconds = (unsigned short) (duration * 1e6);
  vive->system->TriggerHapticPulse(controller->id, axis, uSeconds);
}

void* viveControllerGetModel(void* headset, Controller* controller, ControllerModelFormat* format) {
  Vive* vive = (Vive*) headset;

  *format = CONTROLLER_MODEL_OPENVR;

  // Return the model if it's already loaded
  OpenVRModel* vrModel = &vive->deviceModels[controller->id];
  if (vrModel->isLoaded) {
    return vrModel;
  }

  // Get model name
  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  vive->system->GetStringTrackedDeviceProperty(controller->id, renderModelNameProperty, renderModelName, 1024, NULL);

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

  lovrGraphicsPushCanvas();
  lovrGraphicsSetViewport(0, 0, vive->renderWidth, vive->renderHeight);
  vive->isRendering = 1;
  vive->compositor->WaitGetPoses(vive->renderPoses, 16, NULL, 0);

  // Head transform
  matrix = vive->renderPoses[vive->headsetIndex].mDeviceToAbsoluteTracking.m;
  mat4_invert(mat4_fromMat34(headMatrix, matrix));

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    // Eye transform
    matrix = vive->system->GetEyeToHeadTransform(eye).m;
    mat4_invert(mat4_fromMat34(eyeMatrix, matrix));
    mat4 transformMatrix = mat4_multiply(eyeMatrix, headMatrix);

    // Projection
    matrix = vive->system->GetProjectionMatrix(eye, vive->clipNear, vive->clipFar).m;
    mat4_fromMat44(projectionMatrix, matrix);

    // Render
    glEnable(GL_MULTISAMPLE);
    lovrGraphicsBindFramebuffer(vive->framebuffer);
    lovrGraphicsClear(1, 1);
    lovrGraphicsPush();
    lovrGraphicsOrigin();
    lovrGraphicsMatrixTransform(transformMatrix);
    lovrGraphicsSetProjectionRaw(projectionMatrix);
    callback(i, userdata);
    lovrGraphicsPop();
    lovrGraphicsBindFramebuffer(0);
    glDisable(GL_MULTISAMPLE);

    // Blit
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vive->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vive->resolveFramebuffer);
    glBlitFramebuffer(0, 0, vive->renderWidth, vive->renderHeight, 0, 0, vive->renderWidth, vive->renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Submit
    uintptr_t texture = (uintptr_t) vive->resolveTexture;
    ETextureType textureType = ETextureType_TextureType_OpenGL;
    Texture_t eyeTexture = { (void*) texture, textureType, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    vive->compositor->Submit(eye, &eyeTexture, NULL, flags);
  }

  vive->isRendering = 0;
  lovrGraphicsPopCanvas();
}
