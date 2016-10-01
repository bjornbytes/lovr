#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef struct {
  int (*isPresent)(void* headset);
  const char* (*getType)(void* headset);
  void (*getClipDistance)(void* headset, float* near, float* far);
  void (*setClipDistance)(void* headset, float near, float far);
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  void (*getOrientation)(void* headset, float* x, float* y, float* z, float* w);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  void (*renderTo)(void* headset, headsetRenderCallback callback, void* userdata);
} HeadsetInterface;

typedef struct {
  void* state;
  HeadsetInterface* interface;
} Headset;
#endif

void lovrHeadsetInit();
int lovrHeadsetIsPresent();
const char* lovrHeadsetGetType();
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetSetClipDistance(float near, float far);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
