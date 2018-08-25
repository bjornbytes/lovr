#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

// Provided by resources/lovr.js
extern bool webvrInit(float offset, void (*added)(uint32_t id), void (*removed)(uint32_t id), void (*pressed)(uint32_t, ControllerButton button), void (*released)(uint32_t id, ControllerButton button), void (*mount)(bool mounted));
extern void webvrDestroy(void);
extern HeadsetType webvrGetType(void);
extern HeadsetOrigin webvrGetOriginType(void);
extern bool webvrIsMounted(void);
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
extern bool webvrControllerIsConnected(Controller* controller);
extern ControllerHand webvrControllerGetHand(Controller* controller);
extern void webvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern float webvrControllerGetAxis(Controller* controller, ControllerAxis axis);
extern bool webvrControllerIsDown(Controller* controller, ControllerButton button);
extern bool webvrControllerIsTouched(Controller* controller, ControllerButton button);
extern void webvrControllerVibrate(Controller* controller, float duration, float power);
extern ModelData* webvrControllerNewModelData(Controller* controller);
extern void webvrSetRenderCallback(void (*callback)(float*, float*, float*, float*, void*), void* userdata);
extern void webvrUpdate(float dt);

typedef struct {
  vec_controller_t controllers;
  void (*renderCallback)(void*);
} HeadsetState;

static HeadsetState state;

static void onControllerAdded(uint32_t id) {
  Controller* controller = lovrAlloc(Controller, free);
  controller->id = id;
  vec_push(&state.controllers, controller);
  lovrRetain(controller);
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_ADDED,
    .data.controller = { controller, 0 }
  });
}

static void onControllerRemoved(uint32_t id) {
  for (int i = 0; i < state.controllers.length; i++) {
    if (state.controllers.data[i]->id == id) {
      Controller* controller = state.controllers.data[i];
      lovrRetain(controller);
      lovrEventPush((Event) {
        .type = EVENT_CONTROLLER_REMOVED,
        .data.controller = { controller, 0 }
      });
      vec_splice(&state.controllers, i, 1);
      lovrRelease(controller);
      break;
    }
  }
}

static void onControllerPressed(uint32_t id, ControllerButton button) {
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_PRESSED,
    .data.controller = { state.controllers.data[id], button }
  });
}

static void onControllerReleased(uint32_t id, ControllerButton button) {
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_RELEASED,
    .data.controller = { state.controllers.data[id], button }
  });
}

static void onMountChanged(bool mounted) {
  lovrEventPush((Event) {
    .type = EVENT_MOUNT,
    .data.boolean = { mounted }
  });
}

static void onFrame(float* leftView, float* rightView, float* leftProjection, float* rightProjection, void* userdata) {
  int width, height;
  webvrGetDisplayDimensions(&width, &height);

  Camera camera = { .canvas = NULL, .stereo = true };
  memcpy(camera.projection[0], leftProjection, 16 * sizeof(float));
  memcpy(camera.projection[1], rightProjection, 16 * sizeof(float));
  memcpy(camera.viewMatrix[0], leftView, 16 * sizeof(float));
  memcpy(camera.viewMatrix[1], rightView, 16 * sizeof(float));
  lovrGraphicsSetCamera(&camera, true);
  state.renderCallback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static bool webvrDriverInit(float offset, int msaa) {
  vec_init(&state.controllers);

  if (webvrInit(offset, onControllerAdded, onControllerRemoved, onControllerPressed, onControllerReleased, onMountChanged)) {
    state.renderCallback = NULL;
    return true;
  } else {
    return false;
  }
}

static void webvrDriverDestroy() {
  webvrDestroy();
  vec_deinit(&state.controllers);
  memset(&state, 0, sizeof(HeadsetState));
}

Controller** webvrGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

void webvrRenderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  webvrSetRenderCallback(onFrame, userdata);
}

HeadsetInterface lovrHeadsetWebVRDriver = {
  DRIVER_WEBVR,
  webvrDriverInit,
  webvrDriverDestroy,
  webvrGetType,
  webvrGetOriginType,
  webvrIsMounted,
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
  webvrGetControllers,
  webvrControllerIsConnected,
  webvrControllerGetHand,
  webvrControllerGetPose,
  webvrControllerGetAxis,
  webvrControllerIsDown,
  webvrControllerIsTouched,
  webvrControllerVibrate,
  webvrControllerNewModelData,
  webvrRenderTo,
  webvrUpdate
};
