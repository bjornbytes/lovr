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

  float transform[16];

  // keep track of currently hooked window, if any
  GLFWwindow* hookedWindow;

  int mouselook;      //
  double prevCursorX;
  double prevCursorY;


} FakeHeadsetState;

static FakeHeadsetState state;


static void enableMouselook(GLFWwindow* window) {
  if (window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &state.prevCursorX, &state.prevCursorY);
  }
  // track the intent for mouselook, even if no window yet. One might come along ;-)
  state.mouselook = 1;
}

static void disableMouselook(GLFWwindow* window) {
  if(window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
  state.mouselook = 0;
}

static void cursor_enter_callback( GLFWwindow *window, int entered) {
  if (entered) {
    if( !state.mouselook) {
      enableMouselook(window);
    }
  }
}


static void window_focus_callback(GLFWwindow* window, int focused) {
  if (focused) {
    //enableMouselook(window);
  } else {
    disableMouselook(window);
  }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    disableMouselook(window);
  }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
  if (!state.mouselook) {
    return;
  }
  double dx = xpos - state.prevCursorX;
  double dy = ypos - state.prevCursorY;
  state.prevCursorX = xpos;
  state.prevCursorY = ypos;

  const double k = 0.01;
  const double l = 0.01;

  state.yaw -= dx*k;
  state.pitch -= dy*l;

  if (state.pitch < -M_PI/2.0) {
    state.pitch = -M_PI/2.0;
  }
  if (state.pitch > M_PI/2.0) {
    state.pitch = M_PI/2.0;
  }

}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  if (!state.mouselook) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
      enableMouselook(window);
    }
  }
}

// headset can start up without a window, so we poll window existance here
static void check_window_existance()
{
  GLFWwindow *window = lovrGraphicsGetWindow();

  if (window == state.hookedWindow) {
    // no change
    return;
  }

  state.hookedWindow = window;
  // window might be coming or going.
  // If it's coming we'll install our event hooks.
  // If it's going, it's already gone, so no way to uninstall our hooks.
  if( window ) {
    glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetKeyCallback(window, key_callback);

    // can now actually do mouselook!
    if (state.mouselook ) {
      enableMouselook(window);
    } else {
      disableMouselook(window);
    }
  }
}



void lovrHeadsetInit() {

  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  state.FOV = 67.0f * M_PI / 100.0f;
  // TODO: aspect here too?

  mat4_identity(state.transform);

  state.pitch = 0.0;
  state.yaw = 0.0;
  quat_set( state.orientation, 0,0,0,1);

  vec3_set( state.vel, 0,0,0);
  vec3_set( state.pos, 0,0,0);

  // set up controller(s)
  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
  controller->id = state.controllers.length;
  vec_push(&state.controllers, controller);

  lovrEventAddPump(lovrHeadsetPoll);

  state.mouselook = 1;
  state.hookedWindow = NULL;
  state.isInitialized = 1;

  atexit(lovrHeadsetDestroy);
}


void lovrHeadsetDestroy() {
  int i;
  Controller *controller;
  state.isInitialized = 0;

  // would be polite to unhook gracefully, but we're likely
  // being called after glfw is already lone gone...
  // not a big deal in practice.
#if 0
  GLFWwindow *window = lovrGraphicsGetWindow();
  if( window && window ==state.hookedWindow) {
    glfwSetKeyCallback(window, NULL);
    glfwSetWindowFocusCallback(window, NULL);
    glfwSetCursorPosCallback(window, NULL);
    glfwSetMouseButtonCallback(window, NULL);
    glfwSetCursorEnterCallback(window, NULL);
    state.hookedWindow = NULL;
  }
#endif

  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }
  vec_deinit(&state.controllers);
}

void lovrHeadsetPoll() {
  //state.
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
  GLFWwindow* window = lovrGraphicsGetWindow();
  if(window) {
    glfwGetWindowSize(window,width,height);
  }
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
  // TODO: sit->stand transform?
  *x = state.pos[0];
  *y = state.pos[1];
  *z = state.pos[2];
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
    lovrHeadsetGetPosition(x,y,z);
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  // TODO: sit->stand transform?
  *x = state.vel[0];
  *y = state.vel[1];
  *z = state.vel[2];
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




vec_controller_t* lovrHeadsetGetControllers() {
  return &state.controllers;
}

int lovrHeadsetControllerIsPresent(Controller* controller) {
    return 1;
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  // for now, locked to headset
  lovrHeadsetGetPosition(x,y,z);
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  // for now, locked to headset
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, x, y, z);
}


float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.0f;
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  GLFWwindow* window = lovrGraphicsGetWindow();
  if(!window) {
    return 0;
  }
  int b = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT);
  return (b == GLFW_PRESS) ? CONTROLLER_BUTTON_TRIGGER : 0;
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
  GLFWwindow* window = lovrGraphicsGetWindow();
  if(!window) {
    return;
  }

  glfwGetWindowSize(window, &w, &h); 

  float transform[16];
  mat4_perspective(state.projection, state.clipNear, state.clipFar, 67 * M_PI / 180.0, (float)w/h);

  // render
  lovrGraphicsPush();
  float inv[16];
  mat4_set(inv,state.transform);
  mat4_invert(inv);
  lovrGraphicsMatrixTransform(MATRIX_VIEW, inv);

  lovrGraphicsSetProjection(state.projection);
  lovrGraphicsClear(1, 1);
  callback(EYE_LEFT, userdata);
  lovrGraphicsPop();
}


void lovrHeadsetUpdate(float dt)
{
  float k = 4.0f;
  check_window_existance();
  GLFWwindow* w = lovrGraphicsGetWindow();
  if(!w) {
    return;
  }
  float v[3] = {0.0f,0.0f,0.0f};
  if (glfwGetKey(w, GLFW_KEY_W)==GLFW_PRESS || glfwGetKey(w, GLFW_KEY_UP)==GLFW_PRESS) {
    v[2] = -k;
  }
  if (glfwGetKey(w, GLFW_KEY_S)==GLFW_PRESS || glfwGetKey(w, GLFW_KEY_DOWN)==GLFW_PRESS) {
    v[2] = k;
  }
  if (glfwGetKey(w, GLFW_KEY_A)==GLFW_PRESS || glfwGetKey(w, GLFW_KEY_LEFT)==GLFW_PRESS) {
    v[0] = -k;
  }
  if (glfwGetKey(w, GLFW_KEY_D)==GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT)==GLFW_PRESS) {
    v[0] = k;
  }
  if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) {
    v[1] = -k;
  }
  if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    v[1] = k;
  }

  // move
  vec3_scale(v,dt);
  mat4_transformDirection(state.transform, v);
  vec3_add(state.pos, v);

  // update transform
  mat4_identity(state.transform);
  mat4_translate(state.transform, state.pos[0], state.pos[1], state.pos[2]);
  mat4_rotate(state.transform, state.yaw, 0,1,0);
  mat4_rotate(state.transform, state.pitch, 1,0,0);
}

