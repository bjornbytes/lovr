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
  BUTTON_PRIMARY,
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
  AXIS_PRIMARY,
  AXIS_TRIGGER,
  AXIS_THUMBSTICK,
  AXIS_TOUCHPAD,
  AXIS_PINCH,
  AXIS_GRIP,
  MAX_AXES
} DeviceAxis;

typedef enum {
  BONE_THUMB,
  BONE_INDEX,
  BONE_MIDDLE,
  BONE_RING,
  BONE_PINKY,
  BONE_THUMB_NULL,
  BONE_THUMB_1,
  BONE_THUMB_2,
  BONE_THUMB_3,
  BONE_INDEX_1,
  BONE_INDEX_2,
  BONE_INDEX_3,
  BONE_INDEX_4,
  BONE_MIDDLE_1,
  BONE_MIDDLE_2,
  BONE_MIDDLE_3,
  BONE_MIDDLE_4,
  BONE_RING_1,
  BONE_RING_2,
  BONE_RING_3,
  BONE_RING_4,
  BONE_PINKY_1,
  BONE_PINKY_2,
  BONE_PINKY_3,
  BONE_PINKY_4
} DeviceBone;

typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float offset, uint32_t msaa);
  void (*destroy)(void);
  bool (*getName)(char* name, size_t length);
  HeadsetOrigin (*getOriginType)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  double (*getDisplayTime)(void);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  void (*getBoundsDimensions)(float* width, float* depth);
  const float* (*getBoundsGeometry)(uint32_t* count);
  bool (*getPose)(Device device, float* position, float* orientation);
  bool (*getBonePose)(Device device, DeviceBone bone, float* position, float* orientation);
  bool (*getVelocity)(Device device, float* velocity, float* angularVelocity);
  bool (*getAcceleration)(Device device, float* acceleration, float* angularAcceleration);
  bool (*isDown)(Device device, DeviceButton button, bool* down);
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

bool lovrHeadsetInit(HeadsetDriver* drivers, uint32_t count, float offset, uint32_t msaa);
void lovrHeadsetDestroy(void);

void lovrHeadsetFakeKbamBlock(bool block);
