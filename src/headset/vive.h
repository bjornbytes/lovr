typedef char bool;

#include "headset.h"

Headset* viveInit();
void viveGetAngularVelocity(void* headset, float* x, float* y, float* z);
void viveGetOrientation(void* headset, float* x, float* y, float* z, float* w);
void viveGetPosition(void* headset, float* x, float* y, float* z);
const char* viveGetType(void* headset);
void viveGetVelocity(void* headset, float* x, float* y, float* z);
int viveIsPresent(void* headset);
void viveRenderTo(void* headset, headsetRenderCallback callback, void* userdata);
