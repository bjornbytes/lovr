#include "event/event.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "util.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  bool isInitialized;
  HeadsetType type;

  vec_controller_t controllers;

  float clipNear;
  float clipFar;
  float FOV;

  float vel[3];
  float pos[3];

  double yaw;
  double pitch;

  float orientation[4]; // derived from pitch and yaw

  float projection[16]; // projection matrix

  float transform[16];

  // keep track of currently hooked window, if any
  GLFWwindow* hookedWindow;

  bool mouselook;
  double prevCursorX;
  double prevCursorY;
} FakeHeadsetState;

static FakeHeadsetState state;

#ifdef SPACEMOUSE_SUPPORT
// Do this after declaring state
#include "headset/spacemouse.c"
#endif

// fwd declarations
static void fakePoll();

/*
 * callback handlers
 */

static void enableMouselook(GLFWwindow* window) {
  if (window) {
    glfwGetCursorPos(window, &state.prevCursorX, &state.prevCursorY);
  }

  state.mouselook = true;
}

static void disableMouselook(GLFWwindow* window) {
  state.mouselook = false;
}

static void window_focus_callback(GLFWwindow* window, int focused) {
  if (!focused) {
    disableMouselook(window);
  }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
  if (!state.mouselook) {
    return;
  }

  double dx = xpos - state.prevCursorX;
  double dy = ypos - state.prevCursorY;
  state.prevCursorX = xpos;
  state.prevCursorY = ypos;

  const double k = 0.01;
  const double l = 0.01;

  state.yaw -= dx * k;
  state.pitch -= dy * l;

  if (state.pitch < -M_PI / 2.0) {
    state.pitch = -M_PI / 2.0;
  }

  if (state.pitch > M_PI / 2.0) {
    state.pitch = M_PI / 2.0;
  }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  if (!state.mouselook && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    enableMouselook(window);
  } else if (state.mouselook && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    disableMouselook(window);
  }

  // now generate events on fake controllers
  {
    Event ev;
    Controller* controller;
    int i;
    switch (action) {
      case GLFW_PRESS:
        ev.type = EVENT_CONTROLLER_PRESSED;
        break;
      case GLFW_RELEASE:
        ev.type = EVENT_CONTROLLER_RELEASED;
        break;
      default:
        return;
    }

    vec_foreach(&state.controllers, controller, i) {
      if (controller->id == 0) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
          ev.data.controllerpressed.controller = controller;
          ev.data.controllerpressed.button = CONTROLLER_BUTTON_TRIGGER;
          lovrEventPush(ev);
        }
      }
    }
  }
}

// headset can start up without a window, so we poll window existance here
static void check_window_existance() {
  GLFWwindow* window = glfwGetCurrentContext();

  if (window == state.hookedWindow) {
    // no change
    return;
  }

  state.hookedWindow = window;
  // window might be coming or going.
  // If it's coming we'll install our event hooks.
  // If it's going, it's already gone, so no way to uninstall our hooks.
  if (window) {
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);

    // can now actually do mouselook!
    if (state.mouselook) {
      enableMouselook(window);
    } else {
      disableMouselook(window);
    }
  }
}

static bool fakeIsAvailable() {
  return true;
}

static void fakeInit() {
  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  state.FOV = 67.0f * M_PI / 100.0f;
  // TODO: aspect here too?

  mat4_identity(state.transform);

  state.pitch = 0.0;
  state.yaw = 0.0;
  quat_set(state.orientation, 0, 0, 0, 1);

  vec3_set(state.vel, 0, 0, 0);
  vec3_set(state.pos, 0, 0, 0);

  // set up controller(s)
  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
  controller->id = state.controllers.length;
  vec_push(&state.controllers, controller);

  lovrEventAddPump(fakePoll);

  state.mouselook = false;
  state.hookedWindow = NULL;
  state.isInitialized = true;

#ifdef SPACEMOUSE_SUPPORT
  spacemouseInit();
#endif
}

static void fakeDestroy() {
  int i;
  Controller *controller;

  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }

  vec_deinit(&state.controllers);
  state.isInitialized = false;

#ifdef SPACEMOUSE_SUPPORT
  spacemouseDestroy();
#endif
}

static void fakePoll() {
  //
}

static bool fakeIsPresent() {
  return true;
}

static HeadsetType fakeGetType() {
  return HEADSET_FAKE;
}

static HeadsetOrigin fakeGetOriginType() {
  return ORIGIN_HEAD;
}

static bool fakeIsMirrored() {
  return true;
}

static void fakeSetMirrored(bool mirror) {
  //
}

static void fakeGetDisplayDimensions(int* width, int* height) {
  GLFWwindow* window = glfwGetCurrentContext();
  if (window) {
    glfwGetWindowSize(window,width,height);
  }
}

static void fakeGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void fakeSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static float fakeGetBoundsWidth() {
  return 0.0f;
}

static float fakeGetBoundsDepth() {
  return 0.0f;
}

static void fakeGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = state.pos[0];
  *y = state.pos[1];
  *z = state.pos[2];
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static void fakeGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  fakeGetPose(x, y, z, angle, ax, ay, az);
}

static void fakeGetVelocity(float* x, float* y, float* z) {
  *x = state.vel[0];
  *y = state.vel[1];
  *z = state.vel[2];
}

static void fakeGetAngularVelocity(float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

static vec_controller_t* fakeGetControllers() {
  return &state.controllers;
}

static bool fakeControllerIsPresent(Controller* controller) {
  return true;
}

static ControllerHand fakeControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void fakeControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  // for now, locked to headset

  float offset[3];
  vec3_set(offset, 0, 0, -1.0f);

  mat4_transform(state.transform, offset);
  *x = offset[0];
  *y = offset[1];
  *z = offset[2];

  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static float fakeControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.f;
}

static bool fakeControllerIsDown(Controller* controller, ControllerButton button) {
  GLFWwindow* window = glfwGetCurrentContext();
  if(!window) {
    return false;
  }
  int b = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT);
  return (b == GLFW_PRESS) ? CONTROLLER_BUTTON_TRIGGER : false;
}

static bool fakeControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

static void fakeControllerVibrate(Controller* controller, float duration, float power) {
}

static ModelData* fakeControllerNewModelData(Controller* controller) {
  return NULL;
}

static void fakeRenderTo(headsetRenderCallback callback, void* userdata) {
//  float head[16], transform[16], projection[16];

  // TODO: Head transform
  // TODO: Eye transform
  // Projection

  int w,h;
  GLFWwindow* window = glfwGetCurrentContext();
  if(!window) {
    return;
  }

  glfwGetWindowSize(window, &w, &h);

  mat4_perspective(state.projection, state.clipNear, state.clipFar, 67 * M_PI / 180.0, (float)w/h);

  // render
  lovrGraphicsPush();
  float inv[16];
  mat4_set(inv,state.transform);
  mat4_invert(inv);
  lovrGraphicsMatrixTransform(MATRIX_VIEW, inv);

  lovrGraphicsSetProjection(state.projection);
  lovrGraphicsClear(true, true, true);
  callback(EYE_LEFT, userdata);
  lovrGraphicsPop();
}

static void fakeUpdate(float dt) {
  float k = 4.0f;
  check_window_existance();
  GLFWwindow* w = glfwGetCurrentContext();
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
  if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) {
    v[1] = k;
  }
  if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) {
    v[1] = -k;
  }

#ifdef SPACEMOUSE_SUPPORT
  spacemouseUpdate(v);
#endif

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

HeadsetInterface lovrHeadsetFakeDriver = {
  DRIVER_FAKE,
  fakeIsAvailable,
  fakeInit,
  fakeDestroy,
  fakePoll,
  fakeIsPresent,
  fakeGetType,
  fakeGetOriginType,
  fakeIsMirrored,
  fakeSetMirrored,
  fakeGetDisplayDimensions,
  fakeGetClipDistance,
  fakeSetClipDistance,
  fakeGetBoundsWidth,
  fakeGetBoundsDepth,
  fakeGetPose,
  fakeGetEyePose,
  fakeGetVelocity,
  fakeGetAngularVelocity,
  fakeGetControllers,
  fakeControllerIsPresent,
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
