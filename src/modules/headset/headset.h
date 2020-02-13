#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct ModelData;
struct Texture;

typedef enum {
  ORIGIN_HEAD,
  ORIGIN_FLOOR
} HeadsetOrigin;

typedef enum {
  DRIVER_DESKTOP,
  DRIVER_LEAP_MOTION,
  DRIVER_OCULUS,
  DRIVER_OCULUS_MOBILE,
  DRIVER_OPENVR,
  DRIVER_OPENXR,
  DRIVER_WEBVR
} HeadsetDriver;

typedef enum {
  DEVICE_HEAD,
  DEVICE_HAND_LEFT,
  DEVICE_HAND_RIGHT,
  DEVICE_EYE_LEFT,
  DEVICE_EYE_RIGHT,
  MAX_DEVICES
} Device;

typedef enum {
  BUTTON_TRIGGER,
  BUTTON_THUMBSTICK,
  BUTTON_TOUCHPAD,
  BUTTON_GRIP,
  BUTTON_MENU,
  BUTTON_A,
  BUTTON_B,
  BUTTON_X,
  BUTTON_Y,
  BUTTON_PROXIMITY,
  MAX_BUTTONS
} DeviceButton;

typedef enum {
  AXIS_TRIGGER,
  AXIS_THUMBSTICK,
  AXIS_TOUCHPAD,
  AXIS_GRIP,
  MAX_AXES
} DeviceAxis;

// TODO separate pose and velocity into linear/angular components, maybe keep composite ones though
typedef enum {
  TRACKING_POSE,
  TRACKING_VELOCITY,
  TRACKING_BUTTON,
  TRACKING_TOUCH,
  TRACKING_AXIS,
  TRACKING_SKELETON
} TrackingType;

typedef enum {
  STATUS_UNAVAILABLE,
  STATUS_ESTIMATED,
  STATUS_TRACKED
} TrackingStatus;

// Notes:
// - getDisplayFrequency may return 0.f if the information is unavailable.
// - For isDown, changed can be set to false if change information is unavailable or inconvenient.
// - getAxis may write 4 floats to the output value.  The expected number is a constant (see axisCounts in l_headset).
// - In general, most input results should be kept constant between calls to update.

typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float offset, uint32_t msaa);
  void (*destroy)(void);
  bool (*getName)(char* name, size_t length);
  HeadsetOrigin (*getOriginType)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  float (*getDisplayFrequency)(void);
  const float* (*getDisplayMask)(uint32_t* count);
  double (*getDisplayTime)(void);
  uint32_t (*getViewCount)(void);
  bool (*getViewPose)(uint32_t view, float* position, float* orientation);
  bool (*getViewAngles)(uint32_t view, float* left, float* right, float* up, float* down);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  void (*getBoundsDimensions)(float* width, float* depth);
  const float* (*getBoundsGeometry)(uint32_t* count);
  bool (*getPose)(Device device, float* position, float* orientation);
  bool (*getVelocity)(Device device, float* velocity, float* angularVelocity);
  bool (*isDown)(Device device, DeviceButton button, bool* down, bool* changed);
  bool (*isTouched)(Device device, DeviceButton button, bool* touched);
  bool (*getAxis)(Device device, DeviceAxis axis, float* value);
  bool (*vibrate)(Device device, float strength, float duration, float frequency);
  struct ModelData* (*newModelData)(Device device);
  void (*renderTo)(void (*callback)(void*), void* userdata);
  struct Texture* (*getMirrorTexture)(void);
  void (*update)(float dt);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetOculusDriver;
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetOpenXRDriver;
extern HeadsetInterface lovrHeadsetWebVRDriver;
extern HeadsetInterface lovrHeadsetDesktopDriver;
extern HeadsetInterface lovrHeadsetOculusMobileDriver;
extern HeadsetInterface lovrHeadsetLeapMotionDriver;

// Active drivers
extern HeadsetInterface* lovrHeadsetDriver;
extern HeadsetInterface* lovrHeadsetTrackingDrivers;

#define FOREACH_TRACKING_DRIVER(i)\
  for (HeadsetInterface* i = lovrHeadsetTrackingDrivers; i != NULL; i = i->next)

bool lovrHeadsetInit(HeadsetDriver* drivers, size_t count, float offset, uint32_t msaa);
void lovrHeadsetDestroy(void);
