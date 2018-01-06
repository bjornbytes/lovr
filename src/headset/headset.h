#include "data/model.h"
#include "lib/vec/vec.h"
#include "util.h"
#include <stdbool.h>

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
  DRIVER_FAKE,
  DRIVER_OPENVR,
  DRIVER_WEBVR
} HeadsetDriver;

typedef enum {
  HEADSET_UNKNOWN,
  HEADSET_VIVE,
  HEADSET_RIFT,
  HEADSET_WINDOWS_MR,
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

typedef struct {
  HeadsetDriver driverType;
  bool (*isAvailable)();
  void (*init)();
  void (*destroy)();
  void (*poll)();
  bool (*isPresent)();
  HeadsetType (*getType)();
  HeadsetOrigin (*getOriginType)();
  bool (*isMirrored)();
  void (*setMirrored)(bool mirror);
  void (*getDisplayDimensions)(int* width, int* height);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  float (*getBoundsWidth)();
  float (*getBoundsDepth)();
  void (*getPose)(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  void (*getEyePose)(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  void (*getVelocity)(float* x, float* y, float* z);
  void (*getAngularVelocity)(float* x, float* y, float* z);
  vec_controller_t* (*getControllers)();
  bool (*controllerIsPresent)(Controller* controller);
  ControllerHand (*controllerGetHand)(Controller* controller);
  void (*controllerGetPose)(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  float (*controllerGetAxis)(Controller* controller, ControllerAxis axis);
  bool (*controllerIsDown)(Controller* controller, ControllerButton button);
  bool (*controllerIsTouched)(Controller* controller, ControllerButton button);
  void (*controllerVibrate)(Controller* controller, float duration, float power);
  ModelData* (*controllerNewModelData)(Controller* controller);
  void (*renderTo)(headsetRenderCallback callback, void* userdata);
  void (*update)(float dt);
} HeadsetInterface;

// Headset implementations
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetWebVRDriver;
extern HeadsetInterface lovrHeadsetFakeDriver;

void lovrHeadsetInit(HeadsetDriver* drivers, int count);
void lovrHeadsetDestroy();
void lovrHeadsetPoll();
const HeadsetDriver* lovrHeadsetGetDriver();
bool lovrHeadsetIsPresent();
HeadsetType lovrHeadsetGetType();
HeadsetOrigin lovrHeadsetGetOriginType();
bool lovrHeadsetIsMirrored();
void lovrHeadsetSetMirrored(bool mirror);
void lovrHeadsetGetDisplayDimensions(int* width, int* height);
void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar);
void lovrHeadsetSetClipDistance(float clipNear, float clipFar);
float lovrHeadsetGetBoundsWidth();
float lovrHeadsetGetBoundsDepth();
void lovrHeadsetGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
void lovrHeadsetGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
void lovrHeadsetGetVelocity(float* x, float* y, float* z);
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z);
vec_controller_t* lovrHeadsetGetControllers();
bool lovrHeadsetControllerIsPresent(Controller* controller);
ControllerHand lovrHeadsetControllerGetHand(Controller* controller);
void lovrHeadsetControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis);
bool lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button);
bool lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button);
void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power);
ModelData* lovrHeadsetControllerNewModelData(Controller* controller);
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata);
void lovrHeadsetUpdate(float dt);

void lovrControllerDestroy(const Ref* ref);
