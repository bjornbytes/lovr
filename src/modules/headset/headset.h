#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define HAND_JOINT_COUNT 26
#define MAX_LAYERS 10

struct Model;
struct ModelData;
struct Texture;
struct Pass;

typedef struct Layer Layer;

typedef enum {
  DRIVER_SIMULATOR,
  DRIVER_OPENXR,
  DRIVER_WEBXR
} HeadsetDriver;

typedef enum {
  SKELETON_NONE,
  SKELETON_CONTROLLER,
  SKELETON_NATURAL
} ControllerSkeletonMode;

typedef struct {
  HeadsetDriver* drivers;
  size_t driverCount;
  float supersample;
  bool debug;
  bool seated;
  bool mask;
  bool stencil;
  bool antialias;
  bool submitDepth;
  bool overlay;
  uint32_t overlayOrder;
  ControllerSkeletonMode controllerSkeleton;
} HeadsetConfig;

typedef struct {
  bool overlay;
  bool proximity;
  bool passthrough;
  bool refreshRate;
  bool depthSubmission;
  bool eyeTracking;
  bool handTracking;
  bool handTrackingElbow;
  bool keyboardTracking;
  bool viveTrackers;
  bool handModel;
  bool controllerModel;
  bool controllerSkeleton;
  bool layerCube;
  bool layerSphere;
  bool layerCurve;
  bool layerDepthTest;
  bool layerFilter;
} HeadsetFeatures;

typedef enum {
  FOVEATION_NONE,
  FOVEATION_LOW,
  FOVEATION_MEDIUM,
  FOVEATION_HIGH
} FoveationLevel;

typedef enum {
  PASSTHROUGH_OPAQUE,
  PASSTHROUGH_BLEND,
  PASSTHROUGH_ADD,
  PASSTHROUGH_DEFAULT = -1,
  PASSTHROUGH_TRANSPARENT = -2
} PassthroughMode;

typedef enum {
  DEVICE_HEAD,
  DEVICE_FLOOR,
  DEVICE_HAND_LEFT,
  DEVICE_HAND_RIGHT,
  DEVICE_HAND_LEFT_GRIP,
  DEVICE_HAND_RIGHT_GRIP,
  DEVICE_HAND_LEFT_POINT,
  DEVICE_HAND_RIGHT_POINT,
  DEVICE_HAND_LEFT_PINCH,
  DEVICE_HAND_RIGHT_PINCH,
  DEVICE_HAND_LEFT_POKE,
  DEVICE_HAND_RIGHT_POKE,
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
  DEVICE_STYLUS,
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
  BUTTON_NIB,
  MAX_BUTTONS
} DeviceButton;

typedef enum {
  AXIS_TRIGGER,
  AXIS_THUMBSTICK,
  AXIS_TOUCHPAD,
  AXIS_GRIP,
  AXIS_NIB,
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

typedef enum {
  SOURCE_UNKNOWN,
  SOURCE_CONTROLLER,
  SOURCE_HAND
} SkeletonSource;

typedef struct {
  uint32_t width;
  uint32_t height;
  bool stereo;
  bool immutable;
  bool transparent;
  bool filter;
} LayerInfo;

// Notes:
// - init is called immediately, the graphics module may not exist yet
// - start is called after the graphics module is initialized, can be used to set up textures etc.
// - when the graphics module is destroyed, it stops the headset session, so graphics objects can be destroyed
// - getRefreshRate may return 0.f if the information is unavailable.
// - For isDown, changed can be set to false if change information is unavailable or inconvenient.
// - getAxis may write 4 floats to the output value.  The expected number is a constant (see axisCounts in l_headset).
// - In general, most input results should be kept constant between calls to update.

typedef struct HeadsetInterface {
  HeadsetDriver driverType;
  void (*getVulkanPhysicalDevice)(void* instance, uintptr_t physicalDevice);
  uint32_t (*createVulkanInstance)(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr);
  uint32_t (*createVulkanDevice)(void* instance, void* deviceCreateInfo, void* allocator, uintptr_t device, void* getInstanceProcAddr);
  uintptr_t (*getOpenXRInstanceHandle)(void);
  uintptr_t (*getOpenXRSessionHandle)(void);
  bool (*init)(HeadsetConfig* config);
  bool (*start)(void);
  void (*stop)(void);
  void (*destroy)(void);
  bool (*getDriverName)(char* name, size_t length);
  void (*getFeatures)(HeadsetFeatures* features);
  bool (*getName)(char* name, size_t length);
  bool (*isSeated)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  float (*getRefreshRate)(void);
  bool (*setRefreshRate)(float refreshRate);
  const float* (*getRefreshRates)(uint32_t* count);
  void (*getFoveation)(FoveationLevel* level, bool* dynamic);
  bool (*setFoveation)(FoveationLevel level, bool dynamic);
  PassthroughMode (*getPassthrough)(void);
  bool (*setPassthrough)(PassthroughMode mode);
  bool (*isPassthroughSupported)(PassthroughMode mode);
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
  bool (*getSkeleton)(Device device, float* poses, SkeletonSource* source);
  bool (*vibrate)(Device device, float strength, float duration, float frequency);
  void (*stopVibration)(Device device);
  struct ModelData* (*newModelData)(Device device, bool animated);
  bool (*animate)(struct Model* model);
  struct Texture* (*setBackground)(uint32_t width, uint32_t height, uint32_t layers);
  Layer* (*newLayer)(const LayerInfo* info);
  void (*destroyLayer)(void* ref);
  Layer** (*getLayers)(uint32_t* count, bool* main);
  bool (*setLayers)(Layer** layers, uint32_t count, bool main);
  void (*getLayerPose)(Layer* layer, float* position, float* orientation);
  void (*setLayerPose)(Layer* layer, float* position, float* orientation);
  void (*getLayerDimensions)(Layer* layer, float* width, float* height);
  void (*setLayerDimensions)(Layer* layer, float width, float height);
  float (*getLayerCurve)(Layer* layer);
  bool (*setLayerCurve)(Layer* layer, float curve);
  void (*getLayerColor)(Layer* layer, float color[4]);
  void (*setLayerColor)(Layer* layer, float color[4]);
  void (*getLayerViewport)(Layer* layer, int32_t* viewport);
  void (*setLayerViewport)(Layer* layer, int32_t* viewport);
  struct Texture* (*getLayerTexture)(Layer* layer);
  struct Pass* (*getLayerPass)(Layer* layer);
  bool (*getTexture)(struct Texture** texture);
  bool (*getPass)(struct Pass** pass);
  bool (*submit)(void);
  bool (*isActive)(void);
  bool (*isVisible)(void);
  bool (*isFocused)(void);
  bool (*isMounted)(void);
  bool (*update)(double* dt);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetSimulatorDriver;
extern HeadsetInterface lovrHeadsetOpenXRDriver;
extern HeadsetInterface lovrHeadsetWebXRDriver;

// Active driver
extern HeadsetInterface* lovrHeadsetInterface;

bool lovrHeadsetInit(HeadsetConfig* config);
void lovrHeadsetDestroy(void);
void lovrLayerDestroy(void* ref);
