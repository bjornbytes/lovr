#include "event/event.h"
#include "graphics/graphics.h"
#include "lib/math.h"
#include "platform.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

static struct {
  HeadsetType type;
  bool mirrored;
  HeadsetEye mirrorEye;
  float offset;

  vec_controller_t controllers;

  float clipNear;
  float clipFar;

  float position[3];
  float velocity[3];
  float localVelocity[3];
  float angularVelocity[3];

  double yaw;
  double pitch;
  float transform[16];

  double prevCursorX;
  double prevCursorY;
} state;

static void onMouseButton(MouseButton button, ButtonAction action, int x, int y) {
  if (button == MOUSE_RIGHT) {
    Controller* controller; int i;
    vec_foreach(&state.controllers, controller, i) {
      lovrEventPush((Event) {
        .type = action == BUTTON_PRESSED ? EVENT_CONTROLLER_PRESSED : EVENT_CONTROLLER_RELEASED,
        .data.controller = { controller, CONTROLLER_BUTTON_TRIGGER }
      });
    }
  }
}

static bool fakeInit(float offset, int msaa) {
  state.mirrored = true;
  state.offset = offset;

  state.clipNear = 0.1f;
  state.clipFar = 100.f;

  mat4_identity(state.transform);

  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(Controller, free);
  controller->id = 0;
  vec_push(&state.controllers, controller);

  lovrPlatformOnMouseButton(onMouseButton);
  return true;
}

static void fakeDestroy() {
  Controller *controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(controller);
  }
  vec_deinit(&state.controllers);
  memset(&state, 0, sizeof(state));
}

static HeadsetType fakeGetType() {
  return HEADSET_FAKE;
}

static HeadsetOrigin fakeGetOriginType() {
  return ORIGIN_HEAD;
}

static bool fakeIsMounted() {
  return true;
}

static void fakeIsMirrored(bool* mirrored, HeadsetEye* eye) {
  *mirrored = state.mirrored;
  *eye = state.mirrorEye;
}

static void fakeSetMirrored(bool mirror, HeadsetEye eye) {
  state.mirrored = mirror;
  state.mirrorEye = eye;
}

static void fakeGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  int w, h;
  lovrPlatformGetFramebufferSize(&w, &h);
  *width = (uint32_t) w;
  *height = (uint32_t) h;
}

static void fakeGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void fakeSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void fakeGetBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* fakeGetBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static void fakeGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = *y = *z = 0;
  mat4_transform(state.transform, x, y, z);
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static void fakeGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  fakeGetPose(x, y, z, angle, ax, ay, az);
}

static void fakeGetVelocity(float* x, float* y, float* z) {
  *x = state.velocity[0];
  *y = state.velocity[1];
  *z = state.velocity[2];
}

static void fakeGetAngularVelocity(float* x, float* y, float* z) {
  *x = state.angularVelocity[0];
  *y = state.angularVelocity[1];
  *z = state.angularVelocity[2];
}

static Controller** fakeGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

static bool fakeControllerIsConnected(Controller* controller) {
  return true;
}

static ControllerHand fakeControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void fakeControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = 0;
  *y = 0;
  *z = -.75;
  mat4_transform(state.transform, x, y, z);

  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static float fakeControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.f;
}

static bool fakeControllerIsDown(Controller* controller, ControllerButton button) {
  return lovrPlatformIsMouseDown(MOUSE_RIGHT);
}

static bool fakeControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

static void fakeControllerVibrate(Controller* controller, float duration, float power) {
  //
}

static ModelData* fakeControllerNewModelData(Controller* controller) {
  return NULL;
}

static void fakeRenderTo(void (*callback)(void*), void* userdata) {
  if (!state.mirrored) {
    return;
  }

  uint32_t width, height;
  fakeGetDisplayDimensions(&width, &height);
  bool stereo = state.mirrorEye == EYE_BOTH;
  Camera camera = { .canvas = NULL, .viewMatrix = { MAT4_IDENTITY }, .stereo = stereo };
  mat4_perspective(camera.projection[0], state.clipNear, state.clipFar, 67 * M_PI / 180., (float) width / (1 + stereo) / height);
  mat4_multiply(camera.viewMatrix[0], state.transform);
  mat4_invertPose(camera.viewMatrix[0]);
  mat4_set(camera.projection[1], camera.projection[0]);
  mat4_set(camera.viewMatrix[1], camera.viewMatrix[0]);
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static void fakeUpdate(float dt) {
  bool front = lovrPlatformIsKeyDown(KEY_W) || lovrPlatformIsKeyDown(KEY_UP);
  bool back = lovrPlatformIsKeyDown(KEY_S) || lovrPlatformIsKeyDown(KEY_DOWN);
  bool left = lovrPlatformIsKeyDown(KEY_A) || lovrPlatformIsKeyDown(KEY_LEFT);
  bool right = lovrPlatformIsKeyDown(KEY_D) || lovrPlatformIsKeyDown(KEY_RIGHT);
  bool up = lovrPlatformIsKeyDown(KEY_Q);
  bool down = lovrPlatformIsKeyDown(KEY_E);

  float movespeed = 3.f * dt;
  float turnspeed = 3.f * dt;
  double damping = MAX(1 - 20 * dt, 0);

  if (lovrPlatformIsMouseDown(MOUSE_LEFT)) {
    lovrPlatformSetMouseMode(MOUSE_MODE_GRABBED);

    int width, height;
    lovrPlatformGetWindowSize(&width, &height);

    double mx, my;
    lovrPlatformGetMousePosition(&mx, &my);

    if (state.prevCursorX == -1 && state.prevCursorY == -1) {
      state.prevCursorX = mx;
      state.prevCursorY = my;
    }

    double aspect = (double) width / height;
    double dx = (mx - state.prevCursorX) / ((double) width);
    double dy = (my - state.prevCursorY) / ((double) height * aspect);
    state.angularVelocity[0] = dy / dt;
    state.angularVelocity[1] = dx / dt;
    state.prevCursorX = mx;
    state.prevCursorY = my;
  } else {
    lovrPlatformSetMouseMode(MOUSE_MODE_NORMAL);
    vec3_scale(state.angularVelocity, damping);
    state.prevCursorX = state.prevCursorY = -1;
  }

  // Update velocity
  state.localVelocity[0] = left ? -movespeed : (right ? movespeed : state.localVelocity[0]);
  state.localVelocity[1] = up ? movespeed : (down ? -movespeed : state.localVelocity[1]);
  state.localVelocity[2] = front ? -movespeed : (back ? movespeed : state.localVelocity[2]);
  vec3_init(state.velocity, state.localVelocity);
  mat4_transformDirection(state.transform, &state.velocity[0], &state.velocity[1], &state.velocity[2]);
  vec3_scale(state.localVelocity, damping);

  // Update position
  vec3_add(state.position, state.velocity);

  // Update orientation
  state.pitch = CLAMP(state.pitch - state.angularVelocity[0] * turnspeed, -M_PI / 2., M_PI / 2.);
  state.yaw -= state.angularVelocity[1] * turnspeed;

  // Update transform
  mat4_identity(state.transform);
  mat4_translate(state.transform, 0, state.offset, 0);
  mat4_translate(state.transform, state.position[0], state.position[1], state.position[2]);
  mat4_rotate(state.transform, state.yaw, 0, 1, 0);
  mat4_rotate(state.transform, state.pitch, 1, 0, 0);
}

HeadsetInterface lovrHeadsetFakeDriver = {
  DRIVER_FAKE,
  fakeInit,
  fakeDestroy,
  fakeGetType,
  fakeGetOriginType,
  fakeIsMounted,
  fakeIsMirrored,
  fakeSetMirrored,
  fakeGetDisplayDimensions,
  fakeGetClipDistance,
  fakeSetClipDistance,
  fakeGetBoundsDimensions,
  fakeGetBoundsGeometry,
  fakeGetPose,
  fakeGetEyePose,
  fakeGetVelocity,
  fakeGetAngularVelocity,
  fakeGetControllers,
  fakeControllerIsConnected,
  fakeControllerGetHand,
  fakeControllerGetPose,
  fakeControllerGetAxis,
  fakeControllerIsDown,
  fakeControllerIsTouched,
  fakeControllerVibrate,
  fakeControllerNewModelData,
  fakeRenderTo,
  fakeUpdate
};
