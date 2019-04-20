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

typedef struct HeadsetInterface {
  struct HeadsetInterface* next;
  HeadsetDriver driverType;
  bool (*init)(float offset, int msaa);
  void (*destroy)(void);
  bool (*getName)(char* name, size_t length);
  HeadsetOrigin (*getOriginType)(void);
  void (*getDisplayDimensions)(uint32_t* width, uint32_t* height);
  void (*getClipDistance)(float* clipNear, float* clipFar);
  void (*setClipDistance)(float clipNear, float clipFar);
  void (*getBoundsDimensions)(float* width, float* depth);
  const float* (*getBoundsGeometry)(int* count);
  bool (*getPose)(const char* path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  bool (*getVelocity)(const char* path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz);
  bool (*isDown)(const char* path, bool* down);
  bool (*isTouched)(const char* path, bool* touched);
  int (*getAxis)(const char* path, float* x, float* y, float* z);
  bool (*vibrate)(const char* path, float strength, float duration, float frequency);
  struct ModelData* (*newModelData)(const char* path);
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
