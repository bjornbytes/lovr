#include "event/event.h"
#include "graphics/graphics.h"
#include "lib/maf.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

static struct {
  HeadsetType type;
  float offset;

  vec_controller_t controllers;

  float clipNear;
  float clipFar;

  float position[3];
  float velocity[3];
  float localVelocity[3];
  float angularVelocity[3];

  float yaw;
  float pitch;
  float transform[16];

  double prevCursorX;
  double prevCursorY;
} state;

static void onMouseButton(MouseButton button, ButtonAction action) {
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

static bool desktopInit(float offset, int msaa) {
  state.offset = offset;
  state.clipNear = 0.1f;
  state.clipFar = 100.f;

  mat4_identity(state.transform);

  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(Controller);
  controller->id = 0;
  vec_push(&state.controllers, controller);

  lovrPlatformOnMouseButton(onMouseButton);
  return true;
}

static void desktopDestroy(void) {
  Controller *controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(Controller, controller);
  }
  vec_deinit(&state.controllers);
  memset(&state, 0, sizeof(state));
}

static HeadsetType desktopGetType(void) {
  return HEADSET_UNKNOWN;
}

static HeadsetOrigin desktopGetOriginType(void) {
  return ORIGIN_HEAD;
}

static bool desktopIsMounted(void) {
  return true;
}

static void desktopGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  int w, h;
  lovrPlatformGetFramebufferSize(&w, &h);
  *width = (uint32_t) w;
  *height = (uint32_t) h;
}

static void desktopGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void desktopSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void desktopGetBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* desktopGetBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool desktopGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = *y = *z = 0;
  mat4_transform(state.transform, x, y, z);
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
  return true;
}

static bool desktopGetVelocity(float* vx, float* vy, float* vz) {
  *vx = state.velocity[0];
  *vy = state.velocity[1];
  *vz = state.velocity[2];
  return true;
}

static bool desktopGetAngularVelocity(float* vx, float* vy, float* vz) {
  *vx = state.angularVelocity[0];
  *vy = state.angularVelocity[1];
  *vz = state.angularVelocity[2];
  return true;
}

static Controller** desktopGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

static bool desktopControllerIsConnected(Controller* controller) {
  return true;
}

static ControllerHand desktopControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void desktopControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = 0;
  *y = 0;
  *z = -.75f;
  mat4_transform(state.transform, x, y, z);

  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static void desktopControllerGetVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  *vx = *vy = *vz = 0.f;
}

static void desktopControllerGetAngularVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  *vx = *vy = *vz = 0.f;
}

static float desktopControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.f;
}

static bool desktopControllerIsDown(Controller* controller, ControllerButton button) {
  return lovrPlatformIsMouseDown(MOUSE_RIGHT);
}

static bool desktopControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

static void desktopControllerVibrate(Controller* controller, float duration, float power) {
  //
}

static ModelData* desktopControllerNewModelData(Controller* controller) {
  return NULL;
}

static void desktopRenderTo(void (*callback)(void*), void* userdata) {
  uint32_t width, height;
  desktopGetDisplayDimensions(&width, &height);
  Camera camera = { .canvas = NULL, .viewMatrix = { MAT4_IDENTITY }, .stereo = true };
  mat4_perspective(camera.projection[0], state.clipNear, state.clipFar, 67.f * M_PI / 180.f, (float) width / 2.f / height);
  mat4_multiply(camera.viewMatrix[0], state.transform);
  mat4_invertPose(camera.viewMatrix[0]);
  mat4_set(camera.projection[1], camera.projection[0]);
  mat4_set(camera.viewMatrix[1], camera.viewMatrix[0]);
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static void desktopUpdate(float dt) {
  bool front = lovrPlatformIsKeyDown(KEY_W) || lovrPlatformIsKeyDown(KEY_UP);
  bool back = lovrPlatformIsKeyDown(KEY_S) || lovrPlatformIsKeyDown(KEY_DOWN);
  bool left = lovrPlatformIsKeyDown(KEY_A) || lovrPlatformIsKeyDown(KEY_LEFT);
  bool right = lovrPlatformIsKeyDown(KEY_D) || lovrPlatformIsKeyDown(KEY_RIGHT);
  bool up = lovrPlatformIsKeyDown(KEY_Q);
  bool down = lovrPlatformIsKeyDown(KEY_E);

  float movespeed = 3.f * dt;
  float turnspeed = 3.f * dt;
  float damping = MAX(1.f - 20.f * dt, 0);

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

    float aspect = (float) width / height;
    float dx = (float) (mx - state.prevCursorX) / ((float) width);
    float dy = (float) (my - state.prevCursorY) / ((float) height * aspect);
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
  state.pitch = CLAMP(state.pitch - state.angularVelocity[0] * turnspeed, -(float) M_PI / 2.f, (float) M_PI / 2.f);
  state.yaw -= state.angularVelocity[1] * turnspeed;

  // Update transform
  mat4_identity(state.transform);
  mat4_translate(state.transform, 0.f, state.offset, 0.f);
  mat4_translate(state.transform, state.position[0], state.position[1], state.position[2]);
  mat4_rotate(state.transform, state.yaw, 0.f, 1.f, 0.f);
  mat4_rotate(state.transform, state.pitch, 1.f, 0.f, 0.f);
}

HeadsetInterface lovrHeadsetDesktopDriver = {
  .driverType = DRIVER_DESKTOP,
  .init = desktopInit,
  .destroy = desktopDestroy,
  .getType = desktopGetType,
  .getOriginType = desktopGetOriginType,
  .isMounted = desktopIsMounted,
  .getDisplayDimensions = desktopGetDisplayDimensions,
  .getClipDistance = desktopGetClipDistance,
  .setClipDistance = desktopSetClipDistance,
  .getBoundsDimensions = desktopGetBoundsDimensions,
  .getBoundsGeometry = desktopGetBoundsGeometry,
  .getPose = desktopGetPose,
  .getVelocity = desktopGetVelocity,
  .getAngularVelocity = desktopGetAngularVelocity,
  .getControllers = desktopGetControllers,
  .controllerIsConnected = desktopControllerIsConnected,
  .controllerGetHand = desktopControllerGetHand,
  .controllerGetPose = desktopControllerGetPose,
  .controllerGetVelocity = desktopControllerGetVelocity,
  .controllerGetAngularVelocity = desktopControllerGetAngularVelocity,
  .controllerGetAxis = desktopControllerGetAxis,
  .controllerIsDown = desktopControllerIsDown,
  .controllerIsTouched = desktopControllerIsTouched,
  .controllerVibrate = desktopControllerVibrate,
  .controllerNewModelData = desktopControllerNewModelData,
  .renderTo = desktopRenderTo,
  .update = desktopUpdate
};
