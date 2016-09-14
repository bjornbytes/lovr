#include "headset.h"
#include "../util.h"
#include "../glfw.h"
#include "../graphics/graphics.h"
typedef char bool;
#include <openvr_capi.h>
#include <stdio.h>

extern __declspec(dllimport) bool VR_IsHmdPresent();
extern __declspec(dllimport) bool VR_IsRuntimeInstalled();
extern __declspec(dllimport) intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
extern __declspec(dllimport) bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);
extern __declspec(dllimport) intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);

#define DEVICE_COUNT 16

typedef struct {
  struct VR_IVRSystem_FnTable* vrSystem;
  struct VR_IVRCompositor_FnTable* vrCompositor;

  GLuint framebuffer;
  GLuint depthbuffer;
  GLuint colorTexture;

  uint32_t renderWidth;
  uint32_t renderHeight;
} HeadsetState;

static HeadsetState headsetState;

void lovrHeadsetInit() {
  if (!VR_IsHmdPresent()) {
    error("Warning: HMD not found");
  } else if (!VR_IsRuntimeInstalled()) {
    error("Warning: SteamVR not found");
  }

  EVRInitError vrError;
  uint32_t vrHandle = VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);

  if (vrError != EVRInitError_VRInitError_None) {
    error("Problem initializing OpenVR");
    return;
  }

  if (!VR_IsInterfaceVersionValid(IVRSystem_Version)) {
    error("Invalid OpenVR version");
    return;
  }

  char fnTableName[128];
  
  sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
  headsetState.vrSystem = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || headsetState.vrSystem == NULL) {
    error("Problem initializing VRSystem");
    return;
  }

  sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
  headsetState.vrCompositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(fnTableName, &vrError);
  if (vrError != EVRInitError_VRInitError_None || headsetState.vrCompositor == NULL) {
    error("Problem initializing VRCompositor");
    return;
  }

  headsetState.vrSystem->GetRecommendedRenderTargetSize(&headsetState.renderWidth, &headsetState.renderHeight);

  glGenTextures(1, &headsetState.colorTexture);
  glBindTexture(GL_TEXTURE_2D, headsetState.colorTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, headsetState.renderWidth, headsetState.renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  glGenFramebuffers(1, &headsetState.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, headsetState.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, headsetState.colorTexture, 0);

  glGenRenderbuffers(1, &headsetState.depthbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, headsetState.depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, headsetState.renderWidth, headsetState.renderHeight);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, headsetState.depthbuffer);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    error("Framebuffer not complete");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  /*TrackedDevicePose_t poses[DEVICE_COUNT];
  headsetState.vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.f, poses, DEVICE_COUNT);
  TrackedDevicePose_t hmdPose = poses[k_unTrackedDeviceIndex_Hmd];
  *x = hmdPose.mDeviceToAbsoluteTracking.m[2][0];
  *y = hmdPose.mDeviceToAbsoluteTracking.m[2][1];
  *z = hmdPose.mDeviceToAbsoluteTracking.m[2][2];*/
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {

}

int lovrHeadsetIsPresent() {
  return 1;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  /*float (*m)[4];
  TrackedDevicePose_t pose;
  headsetState.vrCompositor->WaitGetPoses(&pose, 1, NULL, 0);

  m = pose.mDeviceToAbsoluteTracking.m;
  float hmdMatrix[16] = {
    m[0][0], m[1][0], m[2][0], 0.0,
    m[0][1], m[1][1], m[2][1], 0.0,
    m[0][2], m[1][2], m[2][2], 0.0,
    m[0][3], m[1][3], m[2][3], 1.0
  };*/

  TrackedDevicePose_t pose;
  headsetState.vrCompositor->WaitGetPoses(&pose, 1, NULL, 0);

  for (int i = 0; i < 2; i++) {
    EVREye eye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;

    /*m = headsetState.vrSystem->GetEyeToHeadTransform(eye).m;
    float eyeMatrix[16] = {
      m[0][0], m[1][0], m[2][0], 0.0,
      m[0][1], m[1][1], m[2][1], 0.0,
      m[0][2], m[1][2], m[2][2], 0.0,
      m[0][3], m[1][3], m[2][3], 1.0
    };

    m = headsetState.vrSystem->GetProjectionMatrix(eye, 0.1f, 30.0f, EGraphicsAPIConvention_API_OpenGL).m;
    float lovrTransform[16] = {
      m[0][0], m[1][0], m[2][0], m[3][0],
      m[0][1], m[1][1], m[2][1], m[3][1],
      m[0][2], m[1][2], m[2][2], m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    };

    matrixMultiply(matrixMultiply(lovrTransform, eyeMatrix), hmdMatrix);

    Shader* shader = lovrGraphicsGetShader();
    int id = lovrShaderGetUniformId(shader, "lovrTransform");
    lovrShaderSendFloatMat4(shader, id, lovrTransform);
    glClearColor(0.5f, 0.0f, 0.5f, 1.0f);
    glEnable(GL_MULTISAMPLE);
    glBindFramebuffer(GL_FRAMEBUFFER, headsetState.framebuffers[i]);
    glViewport(0, 0, headsetState.renderWidth, headsetState.renderHeight);
    lovrGraphicsClear();
    glEnable(GL_DEPTH_TEST);
    callback(i, userdata);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_MULTISAMPLE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, headsetState.framebuffers[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, headsetState.resolveFramebuffers[i]);
    glBlitFramebuffer(0, 0, headsetState.renderWidth, headsetState.renderHeight, 0, 0, headsetState.renderWidth, headsetState.renderHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);*/

    glBindFramebuffer(GL_FRAMEBUFFER, headsetState.framebuffer);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Texture_t eyeTexture = { (void*) headsetState.framebuffer, EGraphicsAPIConvention_API_OpenGL, EColorSpace_ColorSpace_Gamma };
    EVRSubmitFlags flags = EVRSubmitFlags_Submit_Default;
    headsetState.vrCompositor->Submit(eye, &eyeTexture, NULL, flags);
  }
}
