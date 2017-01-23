#include <vendor/vec/vec.h>
#include "util.h"

#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES

typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

typedef enum {
  EYE_LEFT,
  EYE_RIGHT
} HeadsetEye;

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

typedef enum {
  CONTROLLER_MODEL_NONE = 0,
  CONTROLLER_MODEL_OPENVR
} ControllerModelFormat;

typedef struct {
  Ref ref;
  unsigned int id;
} Controller;

typedef vec_t(Controller*) vec_controller_t;

typedef struct {
  void (*destroy)(void* headset);
  char (*isPresent)(void* headset);
  void (*poll)(void* headset);
  const char* (*getType)(void* headset);
  void (*getDisplayDimensions)(void* headset, int* width, int* height);
  void (*getClipDistance)(void* headset, float* near, float* far);
  void (*setClipDistance)(void* headset, float near, float far);
  float (*getBoundsWidth)(void* headset);
  float (*getBoundsDepth)(void* headset);
  void (*getBoundsGeometry)(void* headset, float* geometry);
  char (*isBoundsVisible)(void* headset);
  void (*setBoundsVisible)(void* headset, char visible);
  void (*getPosition)(void* headset, float* x, float* y, float* z);
  void (*getEyePosition)(void* headset, HeadsetEye eye, float* x, float* y, float* z);
  void (*getOrientation)(void* headset, float* angle, float* x, float* y, float* z);
  void (*getVelocity)(void* headset, float* x, float* y, float* z);
  void (*getAngularVelocity)(void* headset, float* x, float* y, float* z);
  vec_controller_t* (*getControllers)(void* headset);
  char (*controllerIsPresent)(void* headset, Controller* controller);
  void (*controllerGetPosition)(void* headset, Controller* controller, float* x, float* y, float* z);
  void (*controllerGetOrientation)(void* headset, Controller* controller, float* angle, float* x, float* y, float* z);
  float (*controllerGetAxis)(void* headset, Controller* controller, ControllerAxis axis);
  int (*controllerIsDown)(void* headset, Controller* controller, ControllerButton button);
  void (*controllerVibrate)(void* headset, Controller* controller, float duration);
  void* (*controllerGetModel)(void* headset, Controller* controller, ControllerModelFormat* format);
  void (*renderTo)(void* headset, headsetRenderCallback callback, void* userdata);
} Headset;

#endif

void lovrHeadsetInit();
void lovrHeadsetDestroy();
void lovrHeadsetPoll();
char lovrHeadsetIsPresent();
const char* lovrHeadsetGetType();
void lovrHeadsetGetDisplayDimensions(int* width, int* height);
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetSetClipDistance(float near, float far);
float lovrHeadsetGetBoundsWidth();
float lovrHeadsetGetBoundsDepth();
void lovrHeadsetGetBoundsGeometry(float* geometry);
char lovrHeadsetIsBoundsVisible();
void lovrHeadsetSetBoundsVisible(char visible);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
vec_controller_t* lovrHeadsetGetControllers();
char lovrHeadsetControllerIsPresent(Controller* controller);
void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z);
void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z);
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis);
int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button);
void lovrHeadsetControllerVibrate(Controller* controller, float duration);
void* lovrHeadsetControllerGetModel(Controller* controller, ControllerModelFormat* format);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);

void lovrControllerDestroy(const Ref* ref);
