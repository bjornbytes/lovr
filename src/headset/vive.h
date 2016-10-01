typedef char bool;

#include "headset.h"

Headset* viveInit();
int viveIsPresent(void* headset);
const char* viveGetType(void* headset);
void viveGetClipDistance(void* headset, float* near, float* far);
void viveSetClipDistance(void* headset, float near, float far);
void viveGetTrackingSize(void* headset, float* width, float* depth);
void viveGetPosition(void* headset, float* x, float* y, float* z);
void viveGetOrientation(void* headset, float* x, float* y, float* z, float* w);
void viveGetVelocity(void* headset, float* x, float* y, float* z);
void viveGetAngularVelocity(void* headset, float* x, float* y, float* z);
void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata);
