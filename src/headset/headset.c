#include "headset.h"
#include "../util.h"
#include "../glfw.h"
typedef char bool;
#include <openvr_capi.h>
#include <stdio.h>

extern __declspec(dllimport) bool VR_IsHmdPresent();
extern __declspec(dllimport) bool VR_IsRuntimeInstalled();
extern __declspec(dllimport) intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
extern __declspec(dllimport) bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);
extern __declspec(dllimport) intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);

typedef struct {
  struct VR_IVRSystem_FnTable* vrSystem;
  GLuint framebuffers[2];
  GLuint renderbuffers[2];
  GLuint textures[2];
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
    error("Problem initializing OpenVR");
    return;
  }

  glGenFramebuffers(2, headsetState.framebuffers);
  glGenRenderbuffers(2, headsetState.renderbuffers);

  for (int i = 0; i < 2; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, headsetState.framebuffers[i]);
    glBindRenderbuffer(GL_RENDERBUFFER, headsetState.framebuffers[i]);
  }
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  TrackedDevicePose_t poses[16];
  headsetState.vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.f, poses, 16);
  TrackedDevicePose_t hmdPose = poses[k_unTrackedDeviceIndex_Hmd];
  *x = hmdPose.mDeviceToAbsoluteTracking.m[2][0];
  *y = hmdPose.mDeviceToAbsoluteTracking.m[2][1];
  *z = hmdPose.mDeviceToAbsoluteTracking.m[2][2];
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {

}

int lovrHeadsetIsPresent() {
  return 1;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
}
