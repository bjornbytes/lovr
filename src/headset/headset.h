#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES

typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef enum {
  CONTROLLER_HAND_LEFT,
  CONTROLLER_HAND_RIGHT
} ControllerHand;

typedef enum {
  CONTROLLER_AXIS_TRIGGER,
  CONTROLLER_AXIS_TOUCHPAD_X,
  CONTROLLER_AXIS_TOUCHPAD_Y
} ControllerAxis;

typedef enum {
  CONTROLLER_BUTTON_SYSTEM,
  CONTROLLER_BUTTON_MENU,
  CONTROLLER_BUTTON_GRIP,
  CONTROLLER_BUTTON_TOUCHPAD
} ControllerButton;

typedef struct {
  ControllerHand hand;
} Controller;

typedef struct {
  char (*isPresent)(void* headset);
  const char* (*getType)(void* headset);
  void (*getDisplayDimensions)(void* headset, int* width, int* height);
  void (*getClipDistance)(void* headset, float* near, float* far);
  void (*setClipDistance)(void* headset, float near, float far);
  void (*getTrackingSize)(void* headset, float* width, float* depth);
  char (*isBoundsVisible)(void* headset);
  void (*setBoundsVisible)(void* headset, char visible);
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  void (*getOrientation)(void* headset, float* w, float* x, float* y, float* z);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  Controller* (*getController)(void* headset, ControllerHand hand);
  char (*controllerIsPresent)(void* headset, Controller* controller);
  void (*controllerGetPosition)(void* headset, Controller* controller, float* x, float* y, float* z);
  void (*controllerGetOrientation)(void* headset, Controller* controller, float* w, float* x, float* y, float* z);
  float (*controllerGetAxis)(void* headset, Controller* controller, ControllerAxis axis);
  int (*controllerIsDown)(void* headset, Controller* controller, ControllerButton button);
  ControllerHand (*controllerGetHand)(void* headset, Controller* controller);
  void (*controllerVibrate)(void* headset, Controller* controller, float duration);
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
void lovrHeadsetGetDisplayDimensions(int* width, int* height);
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetSetClipDistance(float near, float far);
void lovrHeadsetGetTrackingSize(float* width, float* depth);
char lovrHeadsetIsBoundsVisible();
void lovrHeadsetSetBoundsVisible(char isVisible);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
Controller* lovrHeadsetGetController(ControllerHand hand);
char lovrHeadsetControllerIsPresent(Controller* controller);
void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z);
void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z);
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis);
int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button);
ControllerHand lovrHeadsetControllerGetHand(Controller* controller);
void lovrHeadsetControllerVibrate(Controller* controller, float duration);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
