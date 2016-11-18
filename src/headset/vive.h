#include "headset.h"
#include "../glfw.h"

#ifndef LOVR_VIVE_TYPES
#define LOVR_VIVE_TYPES

typedef struct {
  struct VR_IVRSystem_FnTable* vrSystem;
  struct VR_IVRCompositor_FnTable* vrCompositor;
  struct VR_IVRChaperone_FnTable* vrChaperone;

  unsigned int headsetIndex;
  unsigned int controllerIndex[CONTROLLER_HAND_RIGHT + 1];

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
} ViveState;

#endif

Headset* viveInit();
void viveDestroy(void* headset);
char viveIsPresent(void* headset);
const char* viveGetType(void* headset);
void viveGetDisplayDimensions(void* headset, int* width, int* height);
void viveGetClipDistance(void* headset, float* near, float* far);
void viveSetClipDistance(void* headset, float near, float far);
void viveGetTrackingSize(void* headset, float* width, float* depth);
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
void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata);
