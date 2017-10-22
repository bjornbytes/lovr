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


typedef struct  {
    int (*isAvailable)();
    void (*init)();
    void (*destroy)();
    void (*poll)();
    int (*isPresent)();
    HeadsetType (*getType)();
    HeadsetOrigin (*getOriginType)();
    int (*isMirrored)();
    void (*setMirrored)(int mirror);
    void (*getDisplayDimensions)(int* width, int* height);
    void (*getClipDistance)(float* clipNear, float* clipFar);
    void (*setClipDistance)(float clipNear, float clipFar);
    float (*getBoundsWidth)();
    float (*getBoundsDepth)();
    void (*getBoundsGeometry)(float* geometry);
    void (*getPosition)(float* x, float* y, float* z);
    void (*getEyePosition)(HeadsetEye eye, float* x, float* y, float* z);
    void (*getOrientation)(float* angle, float* x, float* y, float* z);
    void (*getVelocity)(float* x, float* y, float* z);
    void (*getAngularVelocity)(float* x, float* y, float* z);
    vec_controller_t* (*getControllers)();
    int (*controllerIsPresent)(Controller* controller);
    ControllerHand (*controllerGetHand)(Controller* controller);
    void (*controllerGetPosition)(Controller* controller, float* x, float* y, float* z);
    void (*controllerGetOrientation)(Controller* controller, float* angle, float* x, float* y, float* z);
    float (*controllerGetAxis)(Controller* controller, ControllerAxis axis);
    int (*controllerIsDown)(Controller* controller, ControllerButton button);
    int (*controllerIsTouched)(Controller* controller, ControllerButton button);
    void (*controllerVibrate)(Controller* controller, float duration, float power);
    ModelData* (*controllerNewModelData)(Controller* controller);
    void (*renderTo)(headsetRenderCallback callback, void* userdata);
    void (*update)(float dt);
} HeadsetInterface;


// headset implementations
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetFakeDriver;


void lovrHeadsetInit();
void lovrHeadsetDestroy();
void lovrHeadsetPoll();
int lovrHeadsetIsPresent();
HeadsetType lovrHeadsetGetType();
HeadsetOrigin lovrHeadsetGetOriginType();
int lovrHeadsetIsMirrored();
void lovrHeadsetSetMirrored(int mirror);
void lovrHeadsetGetDisplayDimensions(int* width, int* height);
void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar);
void lovrHeadsetSetClipDistance(float clipNear, float clipFar);
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
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
void lovrHeadsetUpdate(float dt);


void lovrControllerDestroy(const Ref* ref);
