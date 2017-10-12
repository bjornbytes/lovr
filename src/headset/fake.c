/* vim: set ts=2 sts=2 sw=2: */
#include "headset/fake.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef struct {
  int isInitialized;
//  int isRendering;
//  int isMirrored;

//  unsigned int headsetIndex;
  HeadsetType type;

  vec_controller_t controllers;

  float clipNear;
  float clipFar;
  float FOV;

//  uint32_t renderWidth;
//  uint32_t renderHeight;

  float vel[3];
  float pos[3];

  double yaw;
  double pitch;

  float orientation[4]; // derived from pitch and yaw

  float projection[16]; // projection matrix

  //
  GLFWwindow* window;

  double prevCursorX;
  double prevCursorY;
} FakeHeadsetState;

static FakeHeadsetState state;


static void cursor_enter_callback( GLFWwindow *window, int entered) {
  if (entered) {
  } else {
  }
}


static void window_focus_callback(GLFWwindow* window, int focused) {
  if (focused) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(state.window, &state.prevCursorX, &state.prevCursorY);
  } else {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
  double dx = xpos - state.prevCursorX;
  double dy = ypos - state.prevCursorY;
  state.prevCursorX = xpos;
  state.prevCursorY = ypos;

  state.yaw += dx;
  state.pitch += dy;

  if(state.pitch < -M_PI/2.0) {
    state.pitch = -M_PI/2.0;
  }
  if(state.pitch > M_PI/2.0) {
    state.pitch = M_PI/2.0;
  }

}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
//    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
//        popup_menu();
}


void lovrHeadsetInit() {
  vec_init(&state.controllers);
  state.isInitialized = 1;
  state.window = lovrGraphicsGetWindow();

  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  state.FOV = 67.0f * M_PI / 100.0f;
  // TODO: aspect here too?

  state.pitch = 0.0;
  state.yaw = 0.0;
  quat_set( state.orientation, 0,0,0,1);

  vec3_set( state.vel, 0,0,0);
  vec3_set( state.pos, 0,0,0);

  lovrHeadsetRefreshControllers();
  lovrEventAddPump(lovrHeadsetPoll);

  glfwSetCursorEnterCallback(state.window, cursor_enter_callback);
  glfwSetMouseButtonCallback(state.window, mouse_button_callback);
  glfwSetCursorPosCallback(state.window, cursor_position_callback);
  glfwSetWindowFocusCallback(state.window, window_focus_callback);
  glfwSetKeyCallback(state.window, key_callback);

  // capture the mouse cursor
  glfwSetInputMode(state.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwGetCursorPos(state.window, &state.prevCursorX, &state.prevCursorY);

  atexit(lovrHeadsetDestroy);
}

void lovrHeadsetDestroy() {
  int i;
  Controller *controller;
  state.isInitialized = 0;

  glfwSetKeyCallback(state.window, NULL);
  glfwSetWindowFocusCallback(state.window, NULL);
  glfwSetCursorPosCallback(state.window, NULL);
  glfwSetMouseButtonCallback(state.window, NULL);
  glfwSetCursorEnterCallback(state.window, NULL);

  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }
  vec_deinit(&state.controllers);
}

void lovrHeadsetPoll() {
  //
}

int lovrHeadsetIsPresent() {
    return 1;
}

HeadsetType lovrHeadsetGetType() {
  return HEADSET_FAKE;
}

HeadsetOrigin lovrHeadsetGetOriginType() {
    return ORIGIN_HEAD; // seated
    //return ORIGIN_FLOOR;  // standing
}

int lovrHeadsetIsMirrored() {
  return 0;
}

void lovrHeadsetSetMirrored(int mirror) {
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  glfwGetWindowSize(state.window,width,height);
}


void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

void lovrHeadsetSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;

}

float lovrHeadsetGetBoundsWidth() {
  return 0.0f;
}

float lovrHeadsetGetBoundsDepth() {
  return 0.0f;
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  memset(geometry, 0, 12 * sizeof(float));
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
/*
  float v[3];
  emscripten_vr_get_position(&v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
*/
//TODO
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
/*
  int i = eye == EYE_LEFT ? 0 : 1;
  emscripten_vr_get_eye_offset(i, x, y, z);
  float m[16];
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_multiply(m, mat4_invert(emscripten_vr_get_view_matrix(i)));
  mat4_translate(m, *x, *y, *z);
  *x = m[12];
  *y = m[13];
  *z = m[14];
*/
// TODO
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
#if 0
  float quat[4];
  float m[16];
  emscripten_vr_get_orientation(&quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, x, y, z);
#endif
// TODO
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
#if 0
  float v[3];
  emscripten_vr_get_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
#endif
// TODO
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
#if 0
  float v[3];
  emscripten_vr_get_angular_velocity(&v[0], &v[1], &v[2]);
  mat4_transformDirection(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
#endif
// TODO
}



void lovrHeadsetRefreshControllers() {
}

Controller* lovrHeadsetAddController(unsigned int deviceIndex) {
}

vec_controller_t* lovrHeadsetGetControllers() {
  return &state.controllers;
}

int lovrHeadsetControllerIsPresent(Controller* controller) {
    return 0;
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
#if 0
  float v[3];
  emscripten_vr_get_controller_position(controller->id, &v[0], &v[1], &v[2]);
  mat4_transform(emscripten_vr_get_sitting_to_standing_matrix(), v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
#endif
// TODO
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
#if 0
  float quat[4];
  float m[16];
  emscripten_vr_get_controller_orientation(controller->id, &quat[0], &quat[1], &quat[2], &quat[3]);
  mat4_multiply(mat4_identity(m), emscripten_vr_get_sitting_to_standing_matrix());
  mat4_rotateQuat(m, quat);
  quat_getAngleAxis(quat_fromMat4(quat, m), angle, x, y, z);
#endif
// TODO
}


float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.0f;
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  return 0;
}

int lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button) {
  return 0;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  return NULL;
}

TextureData* lovrHeadsetControllerNewTextureData(Controller* controller) {
  return NULL;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
//  float head[16], transform[16], projection[16];

  // TODO: Head transform
  // TODO: Eye transform
  // Projection
 
  int w,h; 
  glfwGetWindowSize(state.window, &w, &h); 

  float transform[16];
  mat4_perspective(state.projection, state.clipNear, state.clipFar, 67 * M_PI / 180.0, (float)w/h);

  // render
  lovrGraphicsPush();
  //lovrGraphicsMatrixTransform(MATRIX_VIEW, transform);
  lovrGraphicsSetProjection(state.projection);
  lovrGraphicsClear(1, 1);
  callback(EYE_LEFT, userdata);
  lovrGraphicsPop();
}

