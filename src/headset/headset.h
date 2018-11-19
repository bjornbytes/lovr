#include "data/modelData.h"
#include "lib/vec/vec.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef enum {
  EYE_BOTH = -1,
  EYE_LEFT,
  EYE_RIGHT
} HeadsetEye;

typedef enum {
  ORIGIN_HEAD,
  ORIGIN_FLOOR
} HeadsetOrigin;

typedef enum {
  DRIVER_FAKE,
  DRIVER_OCULUS,
  DRIVER_OCULUS_MOBILE,
  DRIVER_OPENVR,
  DRIVER_WEBVR
} HeadsetDriver;

typedef enum {
  HEADSET_UNKNOWN,
  HEADSET_VIVE,
  HEADSET_RIFT,
  HEADSET_GEAR,
  HEADSET_GO,
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
  CONTROLLER_BUTTON_SYSTEM,
  CONTROLLER_BUTTON_MENU,
  CONTROLLER_BUTTON_TRIGGER,
  CONTROLLER_BUTTON_GRIP,
  CONTROLLER_BUTTON_TOUCHPAD,
  CONTROLLER_BUTTON_A,
  CONTROLLER_BUTTON_B,
  CONTROLLER_BUTTON_X,
  CONTROLLER_BUTTON_Y,
  CONTROLLER_BUTTON_UNKNOWN // Must be last item
} ControllerButton;

typedef enum {
  HAND_UNKNOWN,
  HAND_LEFT,
  HAND_RIGHT
} ControllerHand;

typedef struct {
  Ref ref;
  uint32_t id;
} Controller;

typedef vec_t(Controller*) vec_controller_t;

typedef struct {
  HeadsetDriver driverType;
  bool (*init)(float offset, int msaa);
  void (*destroy)();
  HeadsetType (*getType)();
  HeadsetOrigin (*getOriginType)();
  bool (*isMounted)();
  void (*isMirrored)(bool* mirrored, HeadsetEye* eye);
  void (*setMirrored)(bool mirror, HeadsetEye eye);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  void (*getBoundsDimensions)(float* width, float* depth);
  const float* (*getBoundsGeometry)(int* count);
  void (*getPose)(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  void (*getEyePose)(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  void (*getVelocity)(float* vx, float* vy, float* vz);
  void (*getAngularVelocity)(float* vx, float* vy, float* vz);
  Controller** (*getControllers)(uint8_t* count);
  bool (*controllerIsConnected)(Controller* controller);
  ControllerHand (*controllerGetHand)(Controller* controller);
  void (*controllerGetPose)(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  void (*controllerGetVelocity)(Controller* controller, float* vx, float* vy, float* vz);
  void (*controllerGetAngularVelocity)(Controller* controller, float* vx, float* vy, float* vz);
  float (*controllerGetAxis)(Controller* controller, ControllerAxis axis);
  bool (*controllerIsDown)(Controller* controller, ControllerButton button);
  bool (*controllerIsTouched)(Controller* controller, ControllerButton button);
  void (*controllerVibrate)(Controller* controller, float duration, float power);
  ModelData* (*controllerNewModelData)(Controller* controller);
  void (*renderTo)(void (*callback)(void*), void* userdata);
  void (*update)(float dt);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetOculusDriver;
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetWebVRDriver;
extern HeadsetInterface lovrHeadsetFakeDriver;
extern HeadsetInterface lovrHeadsetOculusMobileDriver;

// Active driver
extern HeadsetInterface* lovrHeadsetDriver;

void lovrHeadsetInit(HeadsetDriver* drivers, int count, float offset, int msaa);
void lovrHeadsetDestroy();
