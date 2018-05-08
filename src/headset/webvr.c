#include "headset/headset.h"
#include "graphics/graphics.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

// Provided by resources/lovr.js
extern void webvrInit(void);
extern HeadsetOrigin webvrGetOriginType(void);
extern bool webvrIsMirrored(void);
extern void webvrSetMirrored(bool mirror);
extern void webvrGetDisplayDimensions(int32_t* width, int32_t* height);
extern void webvrGetClipDistance(float* near, float* far);
extern void webvrSetClipDistance(float near, float far);
extern void webvrGetBoundsDimensions(float* width, float* depth);
extern void webvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern void webvrGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern void webvrGetVelocity(float* x, float* y, float* z);
extern void webvrGetAngularVelocity(float* x, float* y, float* z);
extern void webvrRenderTo(void (*callback)(void*), void* userdata);

typedef struct {
  float offset;
  vec_controller_t controllers;
} HeadsetState;

static HeadsetState state;

static bool webvrDriverInit(float offset) {
  state.offset = offset;
  vec_init(&state.controllers);
  webvrInit();
  return true;
}

static void webvrDriverDestroy() {
  vec_deinit(&state.controllers);
  memset(&state, 0, sizeof(HeadsetState));
}

HeadsetInterface lovrHeadsetWebVRDriver = {
  DRIVER_WEBVR,
  webvrDriverInit,
  webvrDriverDestroy,
  NULL, //HeadsetType (*getType)();
  webvrGetOriginType,
  NULL, //bool (*isMounted)();
  webvrIsMirrored,
  webvrSetMirrored,
  webvrGetDisplayDimensions,
  webvrGetClipDistance,
  webvrSetClipDistance,
  webvrGetBoundsDimensions,
  webvrGetPose,
  webvrGetEyePose,
  webvrGetVelocity,
  webvrGetAngularVelocity,
  NULL, //vec_controller_t* (*getControllers)();
  NULL, //bool (*controllerIsConnected)(Controller* controller);
  NULL, //ControllerHand (*controllerGetHand)(Controller* controller);
  NULL, //void (*controllerGetPose)(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
  NULL, //float (*controllerGetAxis)(Controller* controller, ControllerAxis axis);
  NULL, //bool (*controllerIsDown)(Controller* controller, ControllerButton button);
  NULL, //bool (*controllerIsTouched)(Controller* controller, ControllerButton button);
  NULL, //void (*controllerVibrate)(Controller* controller, float duration, float power);
  NULL, //ModelData* (*controllerNewModelData)(Controller* controller);
  webvrRenderTo,
  NULL //void (*update)(float dt);
};
