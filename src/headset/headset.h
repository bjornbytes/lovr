#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef struct {
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  void (*getOrientation)(void* headset, float* x, float* y, float* z, float* w);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  int (*isPresent)(void* headset);
  void (*renderTo)(void* headset, headsetRenderCallback callback, void* userdata);
} HeadsetInterface;

typedef struct {
  void* state;
  HeadsetInterface* interface;
} Headset;
#endif

void lovrHeadsetInit();
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
int lovrHeadsetIsPresent();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
