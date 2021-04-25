#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define HAND_JOINT_COUNT 26

struct Model;
struct ModelData;
struct Texture;

typedef enum {
  DRIVER_DESKTOP,
  DRIVER_OCULUS,
  DRIVER_OPENVR,
  DRIVER_OPENXR,
  DRIVER_VRAPI,
  DRIVER_PICO,
  DRIVER_WEBXR
} HeadsetDriver;

typedef enum {
  ORIGIN_HEAD,
  ORIGIN_FLOOR
} HeadsetOrigin;

typedef enum {
  DEVICE_HEAD,
  DEVICE_HAND_LEFT,
  DEVICE_HAND_RIGHT,
  DEVICE_HAND_LEFT_POINT,
  DEVICE_HAND_RIGHT_POINT,
  DEVICE_ELBOW_LEFT,
  DEVICE_ELBOW_RIGHT,
  DEVICE_SHOULDER_LEFT,
  DEVICE_SHOULDER_RIGHT,
  DEVICE_CHEST,
  DEVICE_WAIST,
  DEVICE_KNEE_LEFT,
  DEVICE_KNEE_RIGHT,
  DEVICE_FOOT_LEFT,
  DEVICE_FOOT_RIGHT,
  DEVICE_CAMERA,
  DEVICE_KEYBOARD,
  DEVICE_EYE_LEFT,
  DEVICE_EYE_RIGHT,
  DEVICE_BEACON_1,
  DEVICE_BEACON_2,
  DEVICE_BEACON_3,
  DEVICE_BEACON_4,
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

typedef enum {
  JOINT_PALM,
  JOINT_WRIST,
  JOINT_THUMB_METACARPAL,
  JOINT_THUMB_PROXIMAL,
  JOINT_THUMB_DISTAL,
  JOINT_THUMB_TIP,
  JOINT_INDEX_METACARPAL,
  JOINT_INDEX_PROXIMAL,
  JOINT_INDEX_INTERMEDIATE,
  JOINT_INDEX_DISTAL,
  JOINT_INDEX_TIP,
  JOINT_MIDDLE_METACARPAL,
  JOINT_MIDDLE_PROXIMAL,
  JOINT_MIDDLE_INTERMEDIATE,
  JOINT_MIDDLE_DISTAL,
  JOINT_MIDDLE_TIP,
  JOINT_RING_METACARPAL,
  JOINT_RING_PROXIMAL,
  JOINT_RING_INTERMEDIATE,
  JOINT_RING_DISTAL,
  JOINT_RING_TIP,
  JOINT_PINKY_METACARPAL,
  JOINT_PINKY_PROXIMAL,
  JOINT_PINKY_INTERMEDIATE,
  JOINT_PINKY_DISTAL,
  JOINT_PINKY_TIP
} HandJoint;

// Notes:
// - getDisplayFrequency may return 0.f if the information is unavailable.
// - For isDown, changed can be set to false if change information is unavailable or inconvenient.
// - getAxis may write 4 floats to the output value.  The expected number is a constant (see axisCounts in l_headset).
// - In general, most input results should be kept constant between calls to update.

typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float supersample, float offset, uint32_t msaa, bool overlay);
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
  bool (*getSkeleton)(Device device, float* poses);
  bool (*vibrate)(Device device, float strength, float duration, float frequency);
  struct ModelData* (*newModelData)(Device device, bool animated);
  bool (*animate)(Device device, struct Model* model);
  void (*renderTo)(void (*callback)(void*), void* userdata);
  struct Texture* (*getMirrorTexture)(void);
  void (*update)(float dt);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetOculusDriver;
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetOpenXRDriver;
extern HeadsetInterface lovrHeadsetVrApiDriver;
extern HeadsetInterface lovrHeadsetPicoDriver;
extern HeadsetInterface lovrHeadsetWebXRDriver;
extern HeadsetInterface lovrHeadsetDesktopDriver;

// Active drivers
extern HeadsetInterface* lovrHeadsetDisplayDriver;
extern HeadsetInterface* lovrHeadsetTrackingDrivers;

#define FOREACH_TRACKING_DRIVER(i)\
  for (HeadsetInterface* i = lovrHeadsetTrackingDrivers; i != NULL; i = i->next)

bool lovrHeadsetInit(HeadsetDriver* drivers, size_t count, float supersample, float offset, uint32_t msaa, bool overlay);
void lovrHeadsetDestroy(void);
