#include "vive.h"
#include <openvr_capi.h>
#include <stdlib.h>
#include "../util.h"
#include "../graphics/graphics.h"

typedef struct {
  struct VR_IVRSystem_FnTable* vrSystem;
  struct VR_IVRCompositor_FnTable* vrCompositor;

  unsigned int deviceIndex;

  uint32_t renderWidth;
  uint32_t renderHeight;

  GLuint framebuffer;
  GLuint depthbuffer;
  GLuint texture;
} ViveState;

static HeadsetInterface interface = {
  .getPosition = viveGetPosition,
  .getOrientation = viveGetOrientation,
  .getVelocity = viveGetVelocity,
  .getAngularVelocity = viveGetAngularVelocity,
  .isPresent = viveIsPresent,
  .renderTo = viveRenderTo
};

Headset* viveInit() {
  Headset* this = malloc(sizeof(Headset));
  ViveState* state = malloc(sizeof(ViveState));

  this->state = &state;
  this->interface = &interface;

  if (!VR_IsHmdPresent()) {
    error("Warning: HMD not found");
  } else if (!VR_IsRuntimeInstalled()) {
    error("Warning: SteamVR not found");
  }

  EVRInitError vrError;
  uint32_t vrHandle = VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

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

  state->deviceIndex = k_unTrackedDeviceIndex_Hmd;

  state->vrSystem->GetRecommendedRenderTargetSize(&state->renderWidth, &state->renderHeight);

  glGenFramebuffers(1, &state->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);

  glGenRenderbuffers(1, &state->depthbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, state->depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, state->renderWidth, state->renderHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, state->depthbuffer);

  glGenTextures(1, &state->texture);
  glBindTexture(GL_TEXTURE_2D, state->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, state->renderWidth, state->renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    error("framebuffer not complete");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return this;
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

// TODO convert matrix to quaternion!
void viveGetOrientation(void* headset, float* x, float* y, float *z, float* w) {
  *x = *y = *z = *w = 0.f;
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

int viveIsPresent(void* headset) {
  Headset* this = (Headset*) headset;
  ViveState* state = this->state;

  return (int) state->vrSystem->IsTrackedDeviceConnected(state->deviceIndex);
}

void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata) {
  Headset* this = headset;
  ViveState* state = this->state;

  TrackedDevicePose_t pose;
  state->vrCompositor->WaitGetPoses(&pose, 1, NULL, 0);

  float (*m)[4];
  m = pose.mDeviceToAbsoluteTracking.m;
  float viewMatrix[16] = {
    m[0][0], m[1][0], m[2][0], 0.0,
    m[0][1], m[1][1], m[2][1], 0.0,
    m[0][2], m[1][2], m[2][2], 0.0,
    m[0][3], m[1][3], m[2][3], 1.0
  };

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    m = state->vrSystem->GetEyeToHeadTransform(eye).m;
    float eyeMatrix[16] = {
      m[0][0], m[1][0], m[2][0], 0.0,
      m[0][1], m[1][1], m[2][1], 0.0,
      m[0][2], m[1][2], m[2][2], 0.0,
      m[0][3], m[1][3], m[2][3], 1.0
    };

    m = state->vrSystem->GetProjectionMatrix(eye, 0.1f, 30.0f, EGraphicsAPIConvention_API_OpenGL).m;
    float projectionMatrix[16] = {
      m[0][0], m[1][0], m[2][0], m[3][0],
      m[0][1], m[1][1], m[2][1], m[3][1],
      m[0][2], m[1][2], m[2][2], m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    };

    Shader* shader = lovrGraphicsGetShader();
    if (shader) {
      int viewMatrixId = lovrShaderGetUniformId(shader, "viewMatrix");
      int projectionMatrixId = lovrShaderGetUniformId(shader, "projectionMatrix");
      lovrShaderSendFloatMat4(shader, viewMatrixId, viewMatrix);
      lovrShaderSendFloatMat4(shader, projectionMatrixId, projectionMatrix);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);
    glViewport(0, 0, state->renderWidth, state->renderHeight);
    callback(i, userdata);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Texture_t eyeTexture = { (void*) state->texture, EGraphicsAPIConvention_API_OpenGL, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    state->vrCompositor->Submit(eye, &eyeTexture, NULL, flags);
  }
}
