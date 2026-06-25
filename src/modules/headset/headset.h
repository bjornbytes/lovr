#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define HAND_JOINT_COUNT 26
#define BODY_JOINT_COUNT 24
#define MAX_LAYERS 10

struct Model;
struct ModelData;
struct Texture;
struct Pass;

typedef struct Layer Layer;

typedef enum {
  SKELETON_NONE,
  SKELETON_CONTROLLER,
  SKELETON_NATURAL
} ControllerSkeletonMode;

typedef struct {
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
  uint32_t extensionCount;
  char* extensions;
} HeadsetConfig;

typedef struct {
  bool overlay;
  bool battery;
  bool proximity;
  bool passthrough;
  bool refreshRate;
  bool depthSubmission;
  bool eyeTracking;
  bool handTracking;
  bool handTrackingElbow;
  bool bodyTracking;
  bool keyboardTracking;
  bool viveTrackers;
  bool handModel;
  bool controllerModel;
  bool controllerSkeleton;
  bool cubeBackground;
  bool equirectBackground;
  bool layerColor;
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
  DEVICE_HAND_LEFT_PALM,
  DEVICE_HAND_RIGHT_PALM,
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
  DEVICE_BODY,
  MAX_DEVICES
} Device;

typedef enum {
  BUTTON_TRIGGER,
  BUTTON_THUMBSTICK,
  BUTTON_THUMBREST,
  BUTTON_THUMBTAP,
  BUTTON_TOUCHPAD,
  BUTTON_GRIP,
  BUTTON_MENU,
  BUTTON_A,
  BUTTON_B,
  BUTTON_X,
  BUTTON_Y,
  BUTTON_DPAD_UP,
  BUTTON_DPAD_DOWN,
  BUTTON_DPAD_LEFT,
  BUTTON_DPAD_RIGHT,
  BUTTON_BUMPER,
  BUTTON_NIB,
  MAX_BUTTONS
} DeviceButton;

typedef enum {
  AXIS_TRIGGER,
  AXIS_THUMBSTICK,
  AXIS_THUMBREST,
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

bool lovrHeadsetInit(HeadsetConfig* config);
void lovrHeadsetDestroy(void);
bool lovrHeadsetConnect(void);
bool lovrHeadsetIsConnected(void);
bool lovrHeadsetGetName(char* name, size_t length);
bool lovrHeadsetGetDriver(char* name, size_t length);
void lovrHeadsetGetFeatures(HeadsetFeatures* features);
bool lovrHeadsetIsSeated(void);
bool lovrHeadsetStart(void);
void lovrHeadsetStop(void);
bool lovrHeadsetIsActive(void);
bool lovrHeadsetIsVisible(void);
bool lovrHeadsetIsFocused(void);
bool lovrHeadsetIsMounted(void);
bool lovrHeadsetPollEvents(void);
bool lovrHeadsetUpdate(void);
void lovrHeadsetGetDisplayDimensions(uint32_t* width, uint32_t* height);
float* lovrHeadsetGetRefreshRates(uint32_t* count);
float lovrHeadsetGetRefreshRate(void);
bool lovrHeadsetSetRefreshRate(float refreshRate);
void lovrHeadsetGetFoveation(FoveationLevel* level, bool* dynamic);
bool lovrHeadsetSetFoveation(FoveationLevel level, bool dynamic);
bool lovrHeadsetIsPassthroughSupported(PassthroughMode mode);
PassthroughMode lovrHeadsetGetPassthrough(void);
bool lovrHeadsetSetPassthrough(PassthroughMode mode);
uint32_t lovrHeadsetGetViewCount(void);
bool lovrHeadsetGetViewPose(uint32_t view, float* position, float* orientation);
bool lovrHeadsetGetViewAngles(uint32_t view, float* left, float* right, float* up, float* down);
void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar);
void lovrHeadsetSetClipDistance(float clipNear, float clipFar);
void lovrHeadsetGetBoundsDimensions(float* width, float* depth);
bool lovrHeadsetGetPose(Device device, float* position, float* orientation);
bool lovrHeadsetGetVelocity(Device device, float* velocity, float* angularVelocity);
bool lovrHeadsetIsDown(Device device, DeviceButton button, bool* down, bool* changed);
bool lovrHeadsetIsTouched(Device device, DeviceButton button, bool* touched);
bool lovrHeadsetGetAxis(Device device, DeviceAxis axis, float* value);
bool lovrHeadsetGetSkeleton(Device device, float* poses, SkeletonSource* source);
bool lovrHeadsetGetBattery(Device device, float* level, bool* charging);
bool lovrHeadsetVibrate(Device device, float strength, float duration, float frequency);
void lovrHeadsetStopVibration(Device device);
uint64_t* lovrHeadsetGetModelKeys(uint32_t* count);
struct ModelData* lovrHeadsetNewModelData(uint64_t key);
bool lovrHeadsetGetModelPose(struct Model* model, float* position, float* orientation);
bool lovrHeadsetAnimate(struct Model* model);
struct Texture* lovrHeadsetSetBackground(uint32_t width, uint32_t height, uint32_t layers);
Layer** lovrHeadsetGetLayers(uint32_t* count, bool* main);
bool lovrHeadsetSetLayers(Layer** layers, uint32_t count, bool main);
bool lovrHeadsetGetTexture(struct Texture** texture);
bool lovrHeadsetGetPass(struct Pass** pass);
bool lovrHeadsetSubmit(void);
void lovrHeadsetSetPose(Device device, float* position, float* orientation);
void lovrHeadsetSetButton(Device device, DeviceButton button, bool down);

// Layer

typedef struct {
  uint32_t width;
  uint32_t height;
  bool stereo;
  bool immutable;
  bool transparent;
  bool filter;
} LayerInfo;

Layer* lovrLayerCreate(const LayerInfo* info);
void lovrLayerDestroy(void* ref);
Device lovrLayerGetOrigin(Layer* layer);
void lovrLayerSetOrigin(Layer* layer, Device device);
void lovrLayerGetPose(Layer* layer, float* position, float* orientation);
void lovrLayerSetPose(Layer* layer, float* position, float* orientation);
void lovrLayerGetDimensions(Layer* layer, float* width, float* height);
void lovrLayerSetDimensions(Layer* layer, float width, float height);
float lovrLayerGetCurve(Layer* layer);
bool lovrLayerSetCurve(Layer* layer, float curve);
void lovrLayerGetColor(Layer* layer, float color[4]);
void lovrLayerSetColor(Layer* layer, float color[4]);
void lovrLayerGetViewport(Layer* layer, int32_t* viewport);
void lovrLayerSetViewport(Layer* layer, int32_t* viewport);
struct Texture* lovrLayerGetTexture(Layer* layer);
struct Pass* lovrLayerGetPass(Layer* layer);

// Private

void lovrHeadsetGetVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice);
uint32_t lovrHeadsetCreateVulkanInstance(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr);
uint32_t lovrHeadsetCreateVulkanDevice(void* instance, void* deviceCreateInfo, void* allocatoor, uintptr_t device, void* getInstanceProcAddr);
double lovrHeadsetGetDisplayTime(void);
double lovrHeadsetGetDisplayPeriod(void);
double lovrHeadsetGetDeltaTime(void);
