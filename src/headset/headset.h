#ifndef LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef enum {
  STATUS_IDLE,
  STATUS_USER_INTERACTION,
  STATUS_USER_INTERACTION_TIMEOUT,
  STATUS_STANDBY,
  STATUS_UNKNOWN,
  STATUS_MAX_ENUM
} DeviceStatus;
#endif

void lovrHeadsetInit();
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
int lovrHeadsetIsPresent();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
