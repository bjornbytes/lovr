#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define HAND_JOINT_COUNT 26

struct Model;
struct ModelData;
struct Texture;
struct Pass;

typedef enum {
  DRIVER_DESKTOP,
  DRIVER_OPENXR,
  DRIVER_WEBXR
} HeadsetDriver;

typedef struct {
  HeadsetDriver* drivers;
  size_t driverCount;
  float supersample;
  float offset;
  bool stencil;
  bool antialias;
  bool submitDepth;
  bool overlay;
} HeadsetConfig;

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
  DEVICE_EYE_GAZE,
  MAX_DEVICES
} Device;

typedef enum {
  BUTTON_TRIGGER,
  BUTTON_THUMBSTICK,
  BUTTON_THUMBREST,
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
// - init is called immediately, the graphics module may not exist yet
// - start is called after the graphics module is initialized, can be used to set up textures etc.
// - graphics module currently calls stop when it's destroyed, which is hacky and should be improved
// - getDisplayFrequency may return 0.f if the information is unavailable.
// - For isDown, changed can be set to false if change information is unavailable or inconvenient.
// - getAxis may write 4 floats to the output value.  The expected number is a constant (see axisCounts in l_headset).
// - In general, most input results should be kept constant between calls to update.

typedef struct HeadsetInterface {
  HeadsetDriver driverType;
  void (*getVulkanPhysicalDevice)(void* instance, uintptr_t physicalDevice);
  uint32_t (*createVulkanInstance)(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr);
  uint32_t (*createVulkanDevice)(void* instance, void* deviceCreateInfo, void* allocator, uintptr_t device, void* getInstanceProcAddr);
  bool (*init)(HeadsetConfig* config);
  void (*start)(void);
  void (*stop)(void);
  void (*destroy)(void);
  bool (*getName)(char* name, size_t length);
  HeadsetOrigin (*getOriginType)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  float (*getDisplayFrequency)(void);
  float* (*getDisplayFrequencies)(uint32_t* count);
  bool (*setDisplayFrequency)(float);
  double (*getDisplayTime)(void);
  double (*getDeltaTime)(void);
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
  struct Texture* (*getTexture)(void);
  struct Pass* (*getPass)(void);
  void (*submit)(void);
  bool (*isFocused)(void);
  double (*update)(void);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetOpenXRDriver;
extern HeadsetInterface lovrHeadsetWebXRDriver;
extern HeadsetInterface lovrHeadsetDesktopDriver;

// Active driver
extern HeadsetInterface* lovrHeadsetInterface;

bool lovrHeadsetInit(HeadsetConfig* config);
void lovrHeadsetDestroy(void);
