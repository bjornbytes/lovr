#include "vive.h"
#include <openvr_capi.h>
#include <stdlib.h>
#include "../util.h"
#include "../graphics/graphics.h"

typedef struct {
  struct VR_IVRSystem_FnTable* vrSystem;
  struct VR_IVRCompositor_FnTable* vrCompositor;
  struct VR_IVRChaperone_FnTable* vrChaperone;

  unsigned int deviceIndex;

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

static HeadsetInterface interface = {
  .getAngularVelocity = viveGetAngularVelocity,
  .getOrientation = viveGetOrientation,
  .getPosition = viveGetPosition,
  .getType = viveGetType,
  .getVelocity = viveGetVelocity,
  .isPresent = viveIsPresent,
  .renderTo = viveRenderTo
};

Headset* viveInit() {
  Headset* this = malloc(sizeof(Headset));
  ViveState* state = malloc(sizeof(ViveState));

  this->state = state;
  this->interface = &interface;

  if (!VR_IsHmdPresent()) {
    error("Warning: HMD not found");
  } else if (!VR_IsRuntimeInstalled()) {
    error("Warning: SteamVR not found");
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    error("Problem initializing OpenVR");
    return NULL;
  }

  if (!VR_IsInterfaceVersionValid(IVRSystem_Version)) {
    error("Invalid OpenVR version");
    return NULL;
  }

  char fnTableName[128];

  sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
  state->vrSystem = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrSystem == NULL) {
    error("Problem initializing VRSystem");
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
  state->vrCompositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrCompositor == NULL) {
    error("Problem initializing VRCompositor");
    return NULL;
  }

  sprintf(fnTableName, "FnTable:%s", IVRChaperone_Version);
  state->vrChaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || state->vrChaperone == NULL) {
    error("Problem initializing VRChaperone");
    return NULL;
  }

  state->deviceIndex = k_unTrackedDeviceIndex_Hmd;
  state->clipNear = 0.1f;
  state->clipFar = 30.f;
  state->vrSystem->GetRecommendedRenderTargetSize(&state->renderWidth, &state->renderHeight);

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
    error("framebuffer not complete");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return this;
}

void viveGetAngularVelocity(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t pose;
  state->vrSystem->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, &pose, 1);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vAngularVelocity.v[0];
  *y = pose.vAngularVelocity.v[1];
  *z = pose.vAngularVelocity.v[2];
}

void viveGetClipDistance(void* headset, float* near, float* far) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  *near = state->clipNear;
  *far = state->clipFar;
}

// TODO convert matrix to quaternion!
void viveGetOrientation(void* headset, float* x, float* y, float *z, float* w) {
  *x = *y = *z = *w = 0.f;
}

void viveGetPosition(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t pose;
  state->vrSystem->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, &pose, 1);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.mDeviceToAbsoluteTracking.m[0][3];
  *y = pose.mDeviceToAbsoluteTracking.m[1][3];
  *z = pose.mDeviceToAbsoluteTracking.m[2][3];
}

const char* viveGetType(void* headset) {
  return "Vive";
}

void viveGetVelocity(void* headset, float* x, float* y, float* z) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;

  ETrackingUniverseOrigin origin = ETrackingUniverseOrigin_TrackingUniverseStanding;
  float secondsInFuture = 0.f;
  TrackedDevicePose_t pose;
  state->vrSystem->GetDeviceToAbsoluteTrackingPose(origin, secondsInFuture, &pose, 1);

  if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
    *x = *y = *z = 0.f;
    return;
  }

  *x = pose.vVelocity.v[0];
  *y = pose.vVelocity.v[1];
  *z = pose.vVelocity.v[2];
}

int viveIsPresent(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;

  return (int) state->vrSystem->IsTrackedDeviceConnected(state->deviceIndex);
}

void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata) {
  Headset* this = headset;
  ViveState* state = this->state;
  float headMatrix[16], eyeMatrix[16], projectionMatrix[16];
  float (*matrix)[4];
  EGraphicsAPIConvention graphicsConvention = EGraphicsAPIConvention_API_OpenGL;
  TrackedDevicePose_t pose;

  state->vrCompositor->WaitGetPoses(&pose, 1, NULL, 0);
  matrix = pose.mDeviceToAbsoluteTracking.m;
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
    lovrGraphicsTransform(transformMatrix);
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

    Texture_t eyeTexture = { (void*) state->resolveTexture, graphicsConvention, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    state->vrCompositor->Submit(eye, &eyeTexture, NULL, flags);
  }
}

void viveSetClipDistance(void* headset, float near, float far) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;
  state->clipNear = near;
  state->clipFar = far;
  // TODO recompute matrix (only if we're rendering?)
}
