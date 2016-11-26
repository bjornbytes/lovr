#include "headset/headset.h"
#include "glfw.h"
#include "openvr.h"

#ifndef LOVR_VIVE_TYPES
#define LOVR_VIVE_TYPES

typedef struct {
  Headset headset;

  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;

  unsigned int headsetIndex;
  unsigned int controllerIndex[CONTROLLER_HAND_RIGHT + 1];

  OpenVRModel deviceModels[16];

  int isRendering;
  TrackedDevicePose_t renderPoses[16];

  Controller* controllers[CONTROLLER_HAND_RIGHT + 1];

  float clipNear;
  float clipFar;

  uint32_t renderWidth;
  uint32_t renderHeight;

  GLuint framebuffer;
  GLuint depthbuffer;
  GLuint texture;
  GLuint resolveFramebuffer;
  GLuint resolveTexture;
} Vive;

#endif

Headset* viveInit();
void viveDestroy(void* headset);
char viveIsPresent(void* headset);
const char* viveGetType(void* headset);
void viveGetDisplayDimensions(void* headset, int* width, int* height);
void viveGetClipDistance(void* headset, float* near, float* far);
void viveSetClipDistance(void* headset, float near, float far);
float viveGetBoundsWidth(void* headset);
float viveGetBoundsDepth(void* headset);
void viveGetBoundsGeometry(void* headset, float* geometry);
char viveIsBoundsVisible(void* headset);
void viveSetBoundsVisible(void* headset, char visible);
void viveGetPosition(void* headset, float* x, float* y, float* z);
void viveGetOrientation(void* headset, float* x, float* y, float* z, float* w);
void viveGetVelocity(void* headset, float* x, float* y, float* z);
void viveGetAngularVelocity(void* headset, float* x, float* y, float* z);
Controller* viveGetController(void* headset, ControllerHand hand);
char viveControllerIsPresent(void* headset, Controller* controller);
void viveControllerGetPosition(void* headset, Controller* controller, float* x, float* y, float* z);
void viveControllerGetOrientation(void* headset, Controller* controller, float* w, float* x, float* y, float* z);
float viveControllerGetAxis(void* headset, Controller* controller, ControllerAxis axis);
int viveControllerIsDown(void* headset, Controller* controller, ControllerButton button);
ControllerHand viveControllerGetHand(void* headset, Controller* controller);
void viveControllerVibrate(void* headset, Controller* controller, float duration);
void* viveControllerGetModel(void* headset, Controller* controller, ControllerModelFormat* format);
void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata);
