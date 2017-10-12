#include "loaders/model.h"
#include "loaders/texture.h"
#include "lib/vec/vec.h"
#include "util.h"

#pragma once

typedef enum {
  EYE_LEFT,
  EYE_RIGHT
} HeadsetEye;

typedef enum {
  ORIGIN_HEAD,
  ORIGIN_FLOOR
} HeadsetOrigin;

typedef enum {
  HEADSET_UNKNOWN,
  HEADSET_VIVE,
  HEADSET_RIFT,
  HEADSET_FAKE
} HeadsetType;

typedef enum {
  CONTROLLER_AXIS_TRIGGER,
  CONTROLLER_AXIS_GRIP,
  CONTROLLER_AXIS_TOUCHPAD_X,
  CONTROLLER_AXIS_TOUCHPAD_Y
} ControllerAxis;

typedef enum {
  CONTROLLER_BUTTON_UNKNOWN,
  CONTROLLER_BUTTON_SYSTEM,
  CONTROLLER_BUTTON_MENU,
  CONTROLLER_BUTTON_TRIGGER,
  CONTROLLER_BUTTON_GRIP,
  CONTROLLER_BUTTON_TOUCHPAD,
  CONTROLLER_BUTTON_JOYSTICK,
  CONTROLLER_BUTTON_A,
  CONTROLLER_BUTTON_B,
  CONTROLLER_BUTTON_X,
  CONTROLLER_BUTTON_Y
} ControllerButton;

typedef enum {
  HAND_UNKNOWN,
  HAND_LEFT,
  HAND_RIGHT
} ControllerHand;

typedef struct {
  Ref ref;
  unsigned int id;
} Controller;

typedef vec_t(Controller*) vec_controller_t;
typedef void (*headsetRenderCallback)(HeadsetEye eye, void* userdata);

void lovrHeadsetInit();
void lovrHeadsetDestroy();
void lovrHeadsetPoll();
int lovrHeadsetIsPresent();
HeadsetType lovrHeadsetGetType();
HeadsetOrigin lovrHeadsetGetOriginType();
int lovrHeadsetIsMirrored();
void lovrHeadsetSetMirrored(int mirror);
void lovrHeadsetGetDisplayDimensions(int* width, int* height);
void lovrHeadsetGetClipDistance(float* near, float* far);
void lovrHeadsetSetClipDistance(float near, float far);
float lovrHeadsetGetBoundsWidth();
float lovrHeadsetGetBoundsDepth();
void lovrHeadsetGetBoundsGeometry(float* geometry);
void lovrHeadsetGetPosition(float* x, float* y, float* z);
void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z);
void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
vec_controller_t* lovrHeadsetGetControllers();
int lovrHeadsetControllerIsPresent(Controller* controller);
ControllerHand lovrHeadsetControllerGetHand(Controller* controller);
void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z);
void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z);
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis);
int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button);
int lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button);
void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power);
ModelData* lovrHeadsetControllerNewModelData(Controller* controller);
TextureData* lovrHeadsetControllerNewTextureData(Controller* controller);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);

void lovrControllerDestroy(const Ref* ref);
