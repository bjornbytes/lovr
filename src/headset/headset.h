#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef struct {
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  void (*getClipDistance)(void* headset, float* near, float* far);
  void (*getOrientation)(void* headset, float* x, float* y, float* z, float* w);
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  const char* (*getType)(void* headset);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  int (*isPresent)(void* headset);
  void (*renderTo)(void* headset, headsetRenderCallback callback, void* userdata);
  void (*setClipDistance)(void* headset, float near, float far);
} HeadsetInterface;

typedef struct {
  void* state;
  HeadsetInterface* interface;
} Headset;
#endif

void lovrHeadsetInit();
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
const char* lovrHeadsetGetType();
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
int lovrHeadsetIsPresent();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
void lovrHeadsetSetClipDistance(float near, float far);
