#ifndef LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);
#endif

void lovrHeadsetInit();
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z);
int lovrHeadsetIsPresent();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
