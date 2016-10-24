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
char viveIsControllerPresent(void* headset, Controller* controller);
void viveGetControllerPosition(void* headset, Controller* controller, float* x, float* y, float* z);
void viveGetControllerOrientation(void* headset, Controller* controller, float* w, float* x, float* y, float* z);
ControllerHand viveGetControllerHand(void* headset, Controller* controller);
void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata);
