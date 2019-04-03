#include "lib/vec/vec.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct ModelData;
struct Texture;

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
  DRIVER_DESKTOP,
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
  HEADSET_WINDOWS_MR
} HeadsetType;

typedef enum {
  PATH_NONE,
  PATH_HEAD,
  PATH_HANDS,
  PATH_LEFT,
  PATH_RIGHT,
  PATH_TRIGGER,
  PATH_TRACKPAD,
  PATH_MENU,
  PATH_GRIP,
  PATH_A,
  PATH_B,
  PATH_X,
  PATH_Y
} Subpath;

typedef union {
  uint8_t pieces[8];
  uint64_t u64;
} Path;

#define MAKE_PATH(...) ((Path) { { __VA_ARGS__ } })
#define PATH_EQ(p, ...) ((p).u64 == MAKE_PATH(__VA_ARGS__).u64)
#define PATH_STARTS_WITH(p, ...) ((MAKE_PATH(__VA_ARGS__).u64 ^ p.u64 & MAKE_PATH(__VA_ARGS__).u64) == 0)

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

typedef struct Controller {
  Ref ref;
  uint32_t id;
  Path path;
} Controller;

typedef vec_t(Controller*) vec_controller_t;

// The interface implemented by headset backends
//   - The 'next' pointer is used internally to create a linked list of tracking drivers.
//   - If the renderTo function is implemented, the backend is a "display" backend.  Only the first
//     successfully initialized display backend will be used, the rest will be ignored.  If this
//     becomes undesirable in the future, we could initialize subsequent display backends in a
//     special "headless" mode to let them know that they won't need to do any rendering.
typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float offset, int msaa);
  void (*destroy)(void);
  HeadsetType (*getType)(void);
  HeadsetOrigin (*getOriginType)(void);
  bool (*isMounted)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  void (*getBoundsDimensions)(float* width, float* depth);
  const float* (*getBoundsGeometry)(int* count);
  bool (*getPose)(Path path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  bool (*getVelocity)(Path path, float* vx, float* vy, float* vz);
  bool (*getAngularVelocity)(Path path, float* vx, float* vy, float* vz);
  bool (*isDown)(Path path, bool* down);
  bool (*isTouched)(Path path, bool* touched);
  int (*getAxis)(Path path, float* x, float* y, float* z);
  bool (*vibrate)(Path path, float strength, float duration, float frequency);
  struct ModelData* (*newModelData)(Path path);
  Controller** (*getControllers)(uint8_t* count);
  bool (*controllerIsConnected)(Controller* controller);
  ControllerHand (*controllerGetHand)(Controller* controller);
  void (*renderTo)(void (*callback)(void*), void* userdata);
  struct Texture* (*getMirrorTexture)(void);
  void (*update)(float dt);
} HeadsetInterface;

// Available drivers
extern HeadsetInterface lovrHeadsetOculusDriver;
extern HeadsetInterface lovrHeadsetOpenVRDriver;
extern HeadsetInterface lovrHeadsetWebVRDriver;
extern HeadsetInterface lovrHeadsetDesktopDriver;
extern HeadsetInterface lovrHeadsetOculusMobileDriver;

// Active drivers
extern HeadsetInterface* lovrHeadsetDriver;
extern HeadsetInterface* lovrHeadsetTrackingDrivers;

#define FOREACH_TRACKING_DRIVER(i)\
  for (HeadsetInterface* i = lovrHeadsetTrackingDrivers; i != NULL; i = i->next)

bool lovrHeadsetInit(HeadsetDriver* drivers, int count, float offset, int msaa);
void lovrHeadsetDestroy(void);
void lovrControllerDestroy(void* ref);
