#include "event/event.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "lib/glfw.h"
#define _USE_MATH_DEFINES
#include <math.h>
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

  float position[3];
  float velocity[3];
  float angularVelocity[3];

  double yaw;
  double pitch;
  float transform[16];

  GLFWwindow* window;
  bool mouselook;
  double prevCursorX;
  double prevCursorY;
  double prevMove;
} FakeHeadsetState;

static FakeHeadsetState state;

static void enableMouselook(GLFWwindow* window) {
  if (window) {
    glfwGetCursorPos(window, &state.prevCursorX, &state.prevCursorY);
  }

  state.mouselook = true;
}

static void disableMouselook(GLFWwindow* window) {
  state.mouselook = false;
  if (glfwGetTime() - state.prevMove > .1) {
    vec3_set(state.angularVelocity, 0, 0, 0);
  }
}

static void onFocus(GLFWwindow* window, int focused) {
  if (!focused) {
    disableMouselook(window);
  }
}

static void onMouseMove(GLFWwindow* window, double xpos, double ypos) {
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
  state.angularVelocity[0] = dy / (t - state.prevMove);
  state.angularVelocity[1] = dx / (t - state.prevMove);

  state.prevMove = t;
}

static void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (!state.mouselook && action == GLFW_PRESS) {
      enableMouselook(window);
    } else if (state.mouselook && action == GLFW_RELEASE) {
      disableMouselook(window);
    }

    return;
  }

  Controller* controller;
  int i;
  vec_foreach(&state.controllers, controller, i) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      lovrEventPush((Event) {
        .type = action == GLFW_PRESS ? EVENT_CONTROLLER_PRESSED : EVENT_CONTROLLER_RELEASED,
        .data.controller = { controller, CONTROLLER_BUTTON_TRIGGER }
      });
    }
  }
}

static void updateWindow() {
  GLFWwindow* window = glfwGetCurrentContext();

  if (window == state.window) {
    return;
  }

  state.window = window;
  if (window) {
    GLFWmousebuttonfun prevMouseButton = glfwSetMouseButtonCallback(window, onMouseButton);
    if (prevMouseButton) glfwSetMouseButtonCallback(window, prevMouseButton);

    GLFWcursorposfun prevMouseMove = glfwSetCursorPosCallback(window, onMouseMove);
    if (prevMouseMove) glfwSetCursorPosCallback(window, prevMouseMove);

    GLFWwindowfocusfun prevFocus = glfwSetWindowFocusCallback(window, onFocus);
    if (prevFocus) glfwSetWindowFocusCallback(window, onFocus);

    if (state.mouselook) {
      enableMouselook(window);
    } else {
      disableMouselook(window);
    }
  }
}

static bool fakeInit(float offset, int msaa) {
  state.mirrored = true;
  state.offset = offset;

  state.clipNear = 0.1f;
  state.clipFar = 100.f;

  state.pitch = 0.0;
  state.yaw = 0.0;
  mat4_identity(state.transform);

  vec3_set(state.velocity, 0, 0, 0);
  vec3_set(state.position, 0, 0, 0);

  vec_init(&state.controllers);
  Controller* controller = lovrAlloc(Controller, free);
  controller->id = 0;
  vec_push(&state.controllers, controller);

  state.mouselook = false;
  state.window = NULL;
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
  updateWindow();

  if (state.window) {
    glfwGetFramebufferSize(state.window, width, height);
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
  *x = state.position[0];
  *y = state.position[1];
  *z = state.position[2];
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
  *y = state.offset;
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
  return state.window ? (glfwGetMouseButton(state.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) : false;
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
  if (!state.window || !state.mirrored) {
    return;
  }

  int width, height;
  fakeGetDisplayDimensions(&width, &height);
  Camera camera = {
    .stereo = true,
    .canvas = NULL,
    .viewport = {
      { 0, 0, width, height },
      { width, 0, width, height }
    }
  };

  mat4_perspective(camera.projection[0], state.clipNear, state.clipFar, 67 * M_PI / 180., (float) width / height);
  mat4_identity(camera.viewMatrix[0]);
  mat4_translate(camera.viewMatrix[0], 0, state.offset, 0);
  mat4_multiply(camera.viewMatrix[0], state.transform);
  mat4_invert(camera.viewMatrix[0]);
  mat4_set(camera.projection[1], camera.projection[0]);
  mat4_set(camera.viewMatrix[1], camera.viewMatrix[0]);
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static void fakeUpdate(float dt) {
  updateWindow();

  if (!state.window) {
    return;
  }

  bool front = glfwGetKey(state.window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(state.window, GLFW_KEY_UP) == GLFW_PRESS;
  bool back = glfwGetKey(state.window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(state.window, GLFW_KEY_DOWN) == GLFW_PRESS;
  bool left = glfwGetKey(state.window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(state.window, GLFW_KEY_LEFT) == GLFW_PRESS;
  bool right = glfwGetKey(state.window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(state.window, GLFW_KEY_RIGHT) == GLFW_PRESS;
  bool up = glfwGetKey(state.window, GLFW_KEY_Q) == GLFW_PRESS;
  bool down = glfwGetKey(state.window, GLFW_KEY_E) == GLFW_PRESS;

  float k = 4.0f * dt;
  state.velocity[0] = left ? -k : (right ? k : 0);
  state.velocity[1] = up ? k : (down ? -k : 0);
  state.velocity[2] = front ? -k : (back ? k : 0);

  mat4_transformDirection(state.transform, &state.velocity[0], &state.velocity[1], &state.velocity[2]);
  vec3_add(state.position, state.velocity);

  if (!state.mouselook) {
    state.pitch = CLAMP(state.pitch - state.angularVelocity[0] * .002 * dt, -M_PI / 2., M_PI / 2.);
    state.yaw -= state.angularVelocity[1] * .002 * dt;
    vec3_scale(state.angularVelocity, pow(.001, dt));
  }

  mat4_identity(state.transform);
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
