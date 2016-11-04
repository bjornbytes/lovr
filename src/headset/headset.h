#include <vendor/vec/vec.h>
#include "util.h"

#ifndef LOVR_HEADSET_TYPES
#define LOVR_HEADSET_TYPES

typedef void (*headsetRenderCallback)(int eyeIndex, void* userdata);

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
void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
vec_controller_t* lovrHeadsetGetControllers();
char lovrHeadsetControllerIsPresent(Controller* controller);
void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z);
void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z);
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis);
int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button);
void lovrHeadsetControllerVibrate(Controller* controller, float duration);
void* lovrHeadsetControllerGetModel(Controller* controller, ControllerModelFormat* format);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);

void lovrControllerDestroy(const Ref* ref);
