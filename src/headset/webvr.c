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
extern void webvrGetDisplayDimensions(uint32_t* width, uint32_t* height);
extern void webvrGetClipDistance(float* near, float* far);
extern void webvrSetClipDistance(float near, float far);
extern void webvrGetBoundsDimensions(float* width, float* depth);
extern const float* webvrGetBoundsGeometry(int* count);
extern bool webvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern bool webvrGetVelocity(float* vx, float* vy, float* vz);
extern bool webvrGetAngularVelocity(float* vx, float* vy, float* vz);
extern bool webvrControllerIsConnected(Controller* controller);
extern ControllerHand webvrControllerGetHand(Controller* controller);
extern void webvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern void webvrControllerGetVelocity(Controller* controller, float* vx, float* vy, float* vz);
extern void webvrControllerGetAngularVelocity(Controller* controller, float* vx, float* vy, float* vz);
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
  Controller* controller = lovrAlloc(Controller);
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
      lovrRelease(Controller, controller);
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
  .driverType = DRIVER_WEBVR,
  .init = webvrDriverInit,
  .destroy = webvrDriverDestroy,
  .getType = webvrGetType,
  .getOriginType = webvrGetOriginType,
  .isMounted = webvrIsMounted,
  .getDisplayDimensions = webvrGetDisplayDimensions,
  .getClipDistance = webvrGetClipDistance,
  .setClipDistance = webvrSetClipDistance,
  .getBoundsDimensions = webvrGetBoundsDimensions,
  .getBoundsGeometry = webvrGetBoundsGeometry,
  .getPose = webvrGetPose,
  .getVelocity = webvrGetVelocity,
  .getAngularVelocity = webvrGetAngularVelocity,
  .getControllers = webvrGetControllers,
  .controllerIsConnected = webvrControllerIsConnected,
  .controllerGetHand = webvrControllerGetHand,
  .controllerGetPose = webvrControllerGetPose,
  .controllerGetVelocity = webvrControllerGetVelocity,
  .controllerGetAngularVelocity = webvrControllerGetAngularVelocity,
  .controllerGetAxis = webvrControllerGetAxis,
  .controllerIsDown = webvrControllerIsDown,
  .controllerIsTouched = webvrControllerIsTouched,
  .controllerVibrate = webvrControllerVibrate,
  .controllerNewModelData = webvrControllerNewModelData,
  .renderTo = webvrRenderTo,
  .update = webvrUpdate
};
