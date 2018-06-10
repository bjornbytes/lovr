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
  HeadsetType type;
  bool mirrored;
  float offset;

  vec_controller_t controllers;

  float clipNear;
  float clipFar;
  float fov;

  float pos[3];
  float vel[3];
  float avel[3];

  double yaw;
  double pitch;

  float orientation[4];
  float transform[16];

  GLFWwindow* hookedWindow;
  Canvas* canvas;

  bool mouselook;
  double prevCursorX;
  double prevCursorY;
  double prevMove;
} FakeHeadsetState;

static FakeHeadsetState state;

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
  if (glfwGetTime() - state.prevMove > .1) {
    vec3_set(state.avel, 0, 0, 0);
  }
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

  const double k = 0.003;
  const double l = 0.003;

  double t = glfwGetTime();

  state.yaw -= dx * k;
  state.pitch = CLAMP(state.pitch - dy * l, -M_PI / 2., M_PI / 2.);
  state.avel[0] = dy / (t - state.prevMove);
  state.avel[1] = dx / (t - state.prevMove);

  state.prevMove = t;
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
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
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

static bool fakeInit(float offset) {
  state.mirrored = true;
  state.offset = offset;

  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  state.fov = 67.0f * M_PI / 100.0f;

  mat4_identity(state.transform);

  state.pitch = 0.0;
  state.yaw = 0.0;
  quat_set(state.orientation, 0, 0, 0, 1);

  vec3_set(state.vel, 0, 0, 0);
  vec3_set(state.pos, 0, 0, 0);

  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(sizeof(Controller), free);
  controller->id = 0;
  vec_push(&state.controllers, controller);

  state.mouselook = false;
  state.hookedWindow = NULL;
  return true;
}

static void fakeDestroy() {
  int i;
  Controller *controller;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(controller);
  }
  vec_deinit(&state.controllers);
  memset(&state, 0, sizeof(FakeHeadsetState));
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

static bool fakeIsMirrored() {
  return state.mirrored;
}

static void fakeSetMirrored(bool mirror) {
  state.mirrored = mirror;
}

static void fakeGetDisplayDimensions(int* width, int* height) {
  GLFWwindow* window = glfwGetCurrentContext();
  if (window) {
    glfwGetFramebufferSize(window, width, height);
    *width /= 2;
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

static void fakeGetBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
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
  *z = -1;
  mat4_transform(state.transform, x, y, z);

  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, ax, ay, az);
}

static float fakeControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.f;
}

static bool fakeControllerIsDown(Controller* controller, ControllerButton button) {
  GLFWwindow* window = glfwGetCurrentContext();
  if (!window) {
    return false;
  }
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  return state == GLFW_PRESS;
}

static bool fakeControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

static void fakeControllerVibrate(Controller* controller, float duration, float power) {
}

static ModelData* fakeControllerNewModelData(Controller* controller) {
  return NULL;
}

static void fakeRenderTo(void (*callback)(void*), void* userdata) {
  if (!state.mirrored) {
    return;
  }

  int width, height;
  fakeGetDisplayDimensions(&width, &height);

  float projection[16];
  mat4_perspective(projection, state.clipNear, state.clipFar, 67 * M_PI / 180., (float) width / height);

  float view[16];
  mat4_identity(view);
  mat4_translate(view, 0, state.offset, 0);
  mat4_multiply(view, state.transform);
  mat4_invert(view);

  lovrGraphicsPushLayer(NULL);
  lovrGraphicsClear(true, true, true, lovrGraphicsGetBackgroundColor(), 1., 0);
  lovrGraphicsSetCamera(projection, view);
  lovrGraphicsSetViewport(0, 0, width, height);
  callback(userdata);
  lovrGraphicsSetViewport(width, 0, width, height);
  callback(userdata);
  lovrGraphicsPopLayer();
}

static void fakeUpdate(float dt) {
  check_window_existance();
  GLFWwindow* w = glfwGetCurrentContext();
  if (!w) {
    return;
  }

  float k = 4.0f * dt;
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

  // move
  mat4_transformDirection(state.transform, &v[0], &v[1], &v[2]);
  vec3_add(state.pos, v);

  if (!state.mouselook) {
    state.pitch = CLAMP(state.pitch - state.avel[0] * .002 * dt, -M_PI / 2., M_PI / 2.);
    state.yaw -= state.avel[1] * .002 * dt;
    state.avel[0] = state.avel[0] + (-state.avel[0]) * (1 - pow(.001, dt));
    state.avel[1] = state.avel[1] + (-state.avel[1]) * (1 - pow(.001, dt));
  }

  // update transform
  mat4_identity(state.transform);
  mat4_translate(state.transform, state.pos[0], state.pos[1], state.pos[2]);
  mat4_rotate(state.transform, state.yaw, 0,1,0);
  mat4_rotate(state.transform, state.pitch, 1,0,0);
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
