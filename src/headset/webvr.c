#include "headset/headset.h"
#include "graphics/graphics.h"
#include "lib/vec/vec.h"
#include <math/mat4.h>
#include <math/quat.h>
#include <emscripten.h>
#include <emscripten/vr.h>

typedef struct {
  headsetRenderCallback renderCallback;
  vec_controller_t controllers;
} HeadsetState;

static HeadsetState state;

static void onRequestAnimationFrame(void* userdata) {
  lovrGraphicsClear(1, 1);

  int width = emscripten_vr_get_display_width();
  int height = emscripten_vr_get_display_height();

  float projection[16];
  float transform[16];
  float sittingToStanding[16];

  mat4_set(sittingToStanding, emscripten_vr_get_sitting_to_standing_matrix());
  mat4_invert(sittingToStanding);

  for (HeadsetEye eye = EYE_LEFT; eye <= EYE_RIGHT; eye++) {
    int isRight = eye == EYE_RIGHT;

    mat4_set(projection, emscripten_vr_get_projection_matrix(isRight));
    mat4_set(transform, emscripten_vr_get_view_matrix(isRight));
    mat4_multiply(transform, sittingToStanding);

    lovrGraphicsPush();
    lovrGraphicsOrigin();
    lovrGraphicsMatrixTransform(transform);
    lovrGraphicsSetProjection(projection);

    if (isRight) {
      lovrGraphicsSetViewport(width / 2, 0, width / 2, height);
    } else {
      lovrGraphicsSetViewport(0, 0, width / 2, height);
    }

    state.renderCallback(eye, userdata);
    lovrGraphicsPop();
  }
}

void lovrHeadsetInit() {
  vec_init(&state.controllers);
  emscripten_vr_init();
  atexit(lovrHeadsetDestroy);
}

void lovrHeadsetDestroy() {
  Controller* controller;
  int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }

  vec_deinit(&state.controllers);
}

void lovrHeadsetPoll() {
  //
}

int lovrHeadsetIsPresent() {
  return emscripten_vr_is_present();
}

const char* lovrHeadsetGetType() {
  return emscripten_vr_get_display_name();
}

int lovrHeadsetIsMirrored() {
  return 0;
}

void lovrHeadsetSetMirrored(int mirror) {
  //
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  *width = emscripten_vr_get_display_width() / 2;
  *height = emscripten_vr_get_display_height();
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  emscripten_vr_get_display_clip_distance(near, far);
}

void lovrHeadsetSetClipDistance(float near, float far) {
  emscripten_vr_set_display_clip_distance(near, far);
}

float lovrHeadsetGetBoundsWidth() {
  return emscripten_vr_get_bounds_width();
}

float lovrHeadsetGetBoundsDepth() {
  return emscripten_vr_get_bounds_depth();
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  // Not supported
}

char lovrHeadsetIsBoundsVisible() {
  return 0; // Not supported
}

void lovrHeadsetSetBoundsVisible(char visible) {
  // Not supported
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_position(&v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
  int i = eye == EYE_LEFT ? 0 : 1;
  emscripten_vr_get_eye_offset(i, x, y, z);
  float m[16];
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix(i));
  mat4_multiply(m, mat4_invert(emscripten_vr_get_view_matrix(i)));
  mat4_translate(m, *x, *y, *z);
  *x = m[12];
  *y = m[13];
  *z = m[14];
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
  float quat[4];
  float m[16];
  emscripten_vr_get_orientation(&quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_angular_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

vec_controller_t* lovrHeadsetGetControllers() {
  int controllerCount = emscripten_vr_get_controller_count();

  while (state.controllers.length > controllerCount) {
    Controller* controller = vec_last(&state.controllers);
    lovrRelease(&controller->ref);
    vec_pop(&state.controllers);
  }

  while (state.controllers.length < controllerCount) {
    Controller* controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
    controller->id = state.controllers.length;
    vec_push(&state.controllers, controller);
  }

  return &state.controllers;
}

int lovrHeadsetControllerIsPresent(Controller* controller) {
  return emscripten_vr_controller_is_present(controller->id);
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  float v[3];
  emscripten_vr_get_controller_position(controller->id, &v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  float quat[4];
  float m[16];
  emscripten_vr_get_controller_orientation(controller->id, &quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  switch (axis) {
    case CONTROLLER_AXIS_TRIGGER: return emscripten_vr_controller_get_axis(controller->id, -1);
    case CONTROLLER_AXIS_TOUCHPAD_X: return emscripten_vr_controller_get_axis(controller->id, 0);
    case CONTROLLER_AXIS_TOUCHPAD_Y: return emscripten_vr_controller_get_axis(controller->id, 1);
    default: return 0;
  }
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
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

void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
  emscripten_vr_controller_vibrate(controller->id, duration * 1000, power);
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  return NULL;
}

TextureData* lovrHeadsetControllerNewTextureData(Controller* controller) {
  return NULL;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  state.renderCallback = callback;
  emscripten_vr_set_render_callback(onRequestAnimationFrame, userdata);
}
