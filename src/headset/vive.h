#include "headset.h"

Headset* viveInit();
void viveDestroy();
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
