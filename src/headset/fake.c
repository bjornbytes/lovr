/* vim: set ts=2 sts=2 sw=2: */
#include "event/event.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "util.h"
//#include "graphics/texture.h"
//#include "lib/glfw.h"
//#include <stdbool.h>
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


// fwd declarations
static void fakePoll();

/*
 * callback handlers
 */

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



/*
 * headset implementation fns
 */


static int fakeIsAvailable()
{
  return 1;
}


static void fakeInit() {

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

  lovrEventAddPump(fakePoll);

  state.mouselook = 1;
  state.hookedWindow = NULL;
  state.isInitialized = 1;

}


static void fakeDestroy() {
  int i;
  Controller *controller;
  state.isInitialized = 0;

  // TODO: unhook lovrEventAddPump ?

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

static void fakePoll() {
}

static int fakeIsPresent() {
    return 1;
}

static HeadsetType fakeGetType() {
  return HEADSET_FAKE;
}

static HeadsetOrigin fakeGetOriginType() {
    return ORIGIN_HEAD; // seated
    //return ORIGIN_FLOOR;  // standing
}

static int fakeIsMirrored() {
  return 0;
}

static void fakeSetMirrored(int mirror) {
}

static void fakeGetDisplayDimensions(int* width, int* height) {
  GLFWwindow* window = lovrGraphicsGetWindow();
  if(window) {
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

static void fakeGetBoundsGeometry(float* geometry) {
  memset(geometry, 0, 12 * sizeof(float));
}

static void fakeGetPosition(float* x, float* y, float* z) {
  // TODO: sit->stand transform?
  *x = state.pos[0];
  *y = state.pos[1];
  *z = state.pos[2];
}

static void fakeGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
    fakeGetPosition(x,y,z);
}

static void fakeGetOrientation(float* angle, float* x, float* y, float* z) {
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, x, y, z);
}

static void fakeGetVelocity(float* x, float* y, float* z) {
  // TODO: sit->stand transform?
  *x = state.vel[0];
  *y = state.vel[1];
  *z = state.vel[2];
}

static void fakeGetAngularVelocity(float* x, float* y, float* z) {
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




static vec_controller_t* fakeGetControllers() {
  return &state.controllers;
}

static int fakeControllerIsPresent(Controller* controller) {
    return 1;
}

static ControllerHand fakeControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void fakeControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  // for now, locked to headset
  fakeGetPosition(x,y,z);
}

static void fakeControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  // for now, locked to headset
  float q[4];
  quat_fromMat4(q, state.transform);
  quat_getAngleAxis(q, angle, x, y, z);
}


static float fakeControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.0f;
}

static int fakeControllerIsDown(Controller* controller, ControllerButton button) {
  GLFWwindow* window = lovrGraphicsGetWindow();
  if(!window) {
    return 0;
  }
  int b = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT);
  return (b == GLFW_PRESS) ? CONTROLLER_BUTTON_TRIGGER : 0;
}

static int fakeControllerIsTouched(Controller* controller, ControllerButton button) {
  return 0;
}

static void fakeControllerVibrate(Controller* controller, float duration, float power) {
}

static ModelData* fakeControllerNewModelData(Controller* controller) {
  return NULL;
}

static TextureData* fakeControllerNewTextureData(Controller* controller) {
  return NULL;
}


static void fakeRenderTo(headsetRenderCallback callback, void* userdata) {
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


static void fakeUpdate(float dt)
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
    v[1] = k;
  }
  if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    v[1] = -k;
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

HeadsetImpl lovrHeadsetFakeDriver = {
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
  fakeGetBoundsGeometry,
  fakeGetPosition,
  fakeGetEyePosition,
  fakeGetOrientation,
  fakeGetVelocity,
  fakeGetAngularVelocity,
  fakeGetControllers,
  fakeControllerIsPresent,
  fakeControllerGetHand,
  fakeControllerGetPosition,
  fakeControllerGetOrientation,
  fakeControllerGetAxis,
  fakeControllerIsDown,
  fakeControllerIsTouched,
  fakeControllerVibrate,
  fakeControllerNewModelData,
  fakeControllerNewTextureData,
  fakeRenderTo,
  fakeUpdate,
};

