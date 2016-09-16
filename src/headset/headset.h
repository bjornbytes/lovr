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
int lovrHeadsetGetDisplayWidth();
int lovrHeadsetGetDisplayHeight();
int lovrHeadsetIsConnected();
DeviceStatus lovrHeadsetGetStatus();
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
