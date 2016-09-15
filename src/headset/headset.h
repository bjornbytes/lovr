#ifndef LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);
#endif

void lovrHeadsetInit();
int lovrHeadsetGetDisplayWidth();
int lovrHeadsetGetDisplayHeight();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
