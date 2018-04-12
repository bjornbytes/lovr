#include "headset/headset.h"
#include "graphics/graphics.h"
#include "lib/vec/vec.h"
#include "math/mat4.h"
#include "math/quat.h"
#include <emscripten.h>
#include <emscripten/vr.h>
#include <stdbool.h>

typedef struct {
  bool initialized;
  void (*renderCallback)(void*);
  vec_controller_t controllers;
  Canvas* canvas;
  float offset;
} HeadsetState;

static HeadsetState state;

static void onRequestAnimationFrame(void* userdata) {
  int width = emscripten_vr_get_display_width();
  int height = emscripten_vr_get_display_height();

  if (!state.canvas) {
    CanvasFlags flags = { .msaa = 0, .depth = true, .stencil = true, .stereo = true, .mipmaps = false };
    state.canvas = lovrCanvasCreate(width, height, FORMAT_RGB, flags);
  }

  Layer layer = { .canvas = state.canvas };

  float sittingToStanding[16];
  mat4_set(sittingToStanding, emscripten_vr_get_sitting_to_standing_matrix());
  if (!emscripten_vr_has_stage()) {
    mat4_translate(sittingToStanding, 0, state.offset, 0);
  }
  mat4_invert(sittingToStanding);

  mat4_set(&layer.projections[0], emscripten_vr_get_projection_matrix(0));
  mat4_set(&layer.projections[16], emscripten_vr_get_projection_matrix(1));
  mat4_set(&layer.views[0], emscripten_vr_get_view_matrix(0));
  mat4_multiply(&layer.views[0], sittingToStanding);
  mat4_set(&layer.views[16], emscripten_vr_get_view_matrix(1));
  mat4_multiply(&layer.views[16], sittingToStanding);

  lovrGraphicsPushLayer(layer);
  lovrGraphicsClear(true, true, true, lovrGraphicsGetBackgroundColor(), 1., 0);
  state.renderCallback(userdata);
  lovrGraphicsPopLayer();
}

static bool webvrInit(float offset) {
  if (state.initialized) return true;
  if (!emscripten_vr_is_present()) return false;
  vec_init(&state.controllers);
  emscripten_vr_init();
  state.offset = offset;
  return true;
}

static void webvrDestroy() {
  if (!state.initialized) return;
  Controller* controller;
  int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(controller);
  }
  vec_deinit(&state.controllers);
  lovrRelease(state.canvas);
  memset(&state, 0, sizeof(HeadsetState));
}

static HeadsetType webvrGetType() {
  return HEADSET_UNKNOWN;
}

static HeadsetOrigin webvrGetOriginType() {
  return emscripten_vr_has_stage() ? ORIGIN_FLOOR : ORIGIN_HEAD;
}

static bool webvrIsMounted() {
  return true;
}

static bool webvrIsMirrored() {
  return true;
}

static void webvrSetMirrored(bool mirror) {
  //
}

static void webvrGetDisplayDimensions(int* width, int* height) {
  *width = emscripten_vr_get_display_width() / 2;
  *height = emscripten_vr_get_display_height();
}

static void webvrGetClipDistance(float* near, float* far) {
  emscripten_vr_get_display_clip_distance(near, far);
}

static void webvrSetClipDistance(float near, float far) {
  emscripten_vr_set_display_clip_distance(near, far);
}

static float webvrGetBoundsWidth() {
  return emscripten_vr_get_bounds_width();
}

static float webvrGetBoundsDepth() {
  return emscripten_vr_get_bounds_depth();
}

static void webvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  float v[3];
  emscripten_vr_get_position(&v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];

  float quat[4];
  float m[16];
  emscripten_vr_get_orientation(&quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, ax, ay, az);
}

static void webvrGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  int i = eye == EYE_LEFT ? 0 : 1;
  emscripten_vr_get_eye_offset(i, x, y, z);
  float m[16];
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_multiply(m, mat4_invert(emscripten_vr_get_view_matrix(i)));
  mat4_translate(m, *x, *y, *z);
  *x = m[12];
  *y = m[13];
  *z = m[14];

  float quat[4];
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, ax, ay, az);
}

static void webvrGetVelocity(float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

static void webvrGetAngularVelocity(float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_angular_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

static vec_controller_t* webvrGetControllers() {
  int controllerCount = emscripten_vr_get_controller_count();

  while (state.controllers.length > controllerCount) {
    Controller* controller = vec_last(&state.controllers);
    lovrRelease(controller);
    vec_pop(&state.controllers);
  }

  while (state.controllers.length < controllerCount) {
    Controller* controller = lovrAlloc(sizeof(Controller), free);
    controller->id = state.controllers.length;
    vec_push(&state.controllers, controller);
  }

  return &state.controllers;
}

static bool webvrControllerIsConnected(Controller* controller) {
  return emscripten_vr_controller_is_present(controller->id);
}

static ControllerHand webvrControllerGetHand(Controller* controller) {
  switch (emscripten_vr_controller_get_hand(controller->id)) {
    case 0: return HAND_UNKNOWN;
    case 1: return HAND_LEFT;
    case 2: return HAND_RIGHT;
    default: return HAND_UNKNOWN;
  }
}

static void webvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  float v[3];
  emscripten_vr_get_controller_position(controller->id, &v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];

  float quat[4];
  float m[16];
  emscripten_vr_get_controller_orientation(controller->id, &quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, ax, ay, az);
}

static float webvrControllerGetAxis(Controller* controller, ControllerAxis axis) {
  switch (axis) {
    case CONTROLLER_AXIS_TRIGGER: return emscripten_vr_controller_get_axis(controller->id, -1);
    case CONTROLLER_AXIS_TOUCHPAD_X: return emscripten_vr_controller_get_axis(controller->id, 0);
    case CONTROLLER_AXIS_TOUCHPAD_Y: return emscripten_vr_controller_get_axis(controller->id, 1);
    default: return 0;
  }
}

static bool webvrControllerIsDown(Controller* controller, ControllerButton button) {
  switch (button) {
    case CONTROLLER_BUTTON_TOUCHPAD:
      return emscripten_vr_controller_is_down(controller->id, 0);

    case CONTROLLER_BUTTON_GRIP:
      return emscripten_vr_controller_is_down(controller->id, 2);

    case CONTROLLER_BUTTON_MENU:
      return emscripten_vr_controller_is_down(controller->id, 3);

    default: return 0;
  }
}

static bool webvrControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

static void webvrControllerVibrate(Controller* controller, float duration, float power) {
  emscripten_vr_controller_vibrate(controller->id, duration * 1000, power);
}

static ModelData* webvrControllerNewModelData(Controller* controller) {
  return NULL;
}

static void webvrRenderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  emscripten_vr_set_render_callback(onRequestAnimationFrame, userdata);
}

static void webvrUpdate(float dt) {
}

HeadsetInterface lovrHeadsetWebVRDriver = {
  DRIVER_WEBVR,
  webvrInit,
  webvrDestroy,
  webvrGetType,
  webvrGetOriginType,
  webvrIsMounted,
  webvrIsMirrored,
  webvrSetMirrored,
  webvrGetDisplayDimensions,
  webvrGetClipDistance,
  webvrSetClipDistance,
  webvrGetBoundsWidth,
  webvrGetBoundsDepth,
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
