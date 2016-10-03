#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES
typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef enum {
  CONTROLLER_HAND_LEFT,
  CONTROLLER_HAND_RIGHT
} ControllerHand;

typedef struct {
  ControllerHand hand;
} Controller;

typedef struct {
  char (*isPresent)(void* headset);
  const char* (*getType)(void* headset);
  void (*getClipDistance)(void* headset, float* near, float* far);
  void (*setClipDistance)(void* headset, float near, float far);
  void (*getTrackingSize)(void* headset, float* width, float* depth);
  char (*isBoundsVisible)(void* headset);
  void (*setBoundsVisible)(void* headset, char visible);
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  void (*getOrientation)(void* headset, float* x, float* y, float* z, float* w);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  Controller* (*getController)(void* headset, ControllerHand hand);
  char (*isControllerPresent)(void* headset, Controller* controller);
  void (*getControllerPosition)(void* headset, Controller* controller, float* x, float* y, float* z);
  void (*getControllerOrientation)(void* headset, Controller* controller, float* w, float* x, float* y, float* z);
  void (*renderTo)(void* headset, headsetRenderCallback callback, void* userdata);
} HeadsetInterface;

typedef struct {
  void* state;
  HeadsetInterface* interface;
} Headset;
#endif

void lovrHeadsetInit();
char lovrHeadsetIsPresent();
const char* lovrHeadsetGetType();
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetSetClipDistance(float near, float far);
void lovrHeadsetGetTrackingSize(float* width, float* depth);
char lovrHeadsetIsBoundsVisible();
void lovrHeadsetSetBoundsVisible(char isVisible);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
Controller* lovrHeadsetGetController(ControllerHand hand);
char lovrHeadsetIsControllerPresent(Controller* controller);
void lovrHeadsetGetControllerPosition(Controller* controller, float* x, float* y, float* z);
void lovrHeadsetGetControllerOrientation(Controller* controller, float* w, float* x, float* y, float* z);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
