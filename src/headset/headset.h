#include "lib/vec/vec.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct ModelData;
struct Texture;

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
  P_NONE,
  P_HEAD,
  P_HAND,
  P_EYE,
  P_LEFT,
  P_RIGHT,
  P_PROXIMITY,
  P_TRIGGER,
  P_TRACKPAD,
  P_JOYSTICK,
  P_MENU,
  P_GRIP,
  P_A,
  P_B,
  P_X,
  P_Y
} Subpath;

typedef union {
  uint8_t p[8];
  uint64_t u64;
} Path;

#define MAKE_PATH(...) ((Path) { { __VA_ARGS__ } })
#define PATH_EQ(p, ...) ((p).u64 == MAKE_PATH(__VA_ARGS__).u64)
#define PATH_STARTS_WITH(p, ...) ((MAKE_PATH(__VA_ARGS__).u64 ^ p.u64 & MAKE_PATH(__VA_ARGS__).u64) == 0)

typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float offset, int msaa);
  void (*destroy)(void);
  const char* (*getName)(void);
  HeadsetOrigin (*getOriginType)(void);
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
