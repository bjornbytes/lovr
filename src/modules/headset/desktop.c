#include "headset/headset.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/platform.h"
#include "core/util.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

static struct {
  bool initialized;

  float LOVR_ALIGN(16) position[4];
  float LOVR_ALIGN(16) velocity[4];
  float LOVR_ALIGN(16) localVelocity[4];
  float LOVR_ALIGN(16) angularVelocity[4];
  float LOVR_ALIGN(16) headTransform[16];
  float LOVR_ALIGN(16) leftHandTransform[16];

  double prevCursorX;
  double prevCursorY;

  float offset;
  float clipNear;
  float clipFar;
  float pitch;
  float yaw;
} state;

static bool desktop_init(float offset, uint32_t msaa) {
  state.offset = offset;
  state.clipNear = 0.1f;
  state.clipFar = 100.f;

  if (!state.initialized) {
    mat4_identity(state.headTransform);
    mat4_identity(state.leftHandTransform);
    state.initialized = true;
  }
  return true;
}

static void desktop_destroy(void) {
}

static bool desktop_getName(char* name, size_t length) {
  strncpy(name, "Simulator", length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin desktop_getOriginType(void) {
  return ORIGIN_HEAD;
}

static double desktop_getDisplayTime(void) {
  return lovrPlatformGetTime();
}

static void desktop_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  int w, h;
  lovrPlatformGetFramebufferSize(&w, &h);
  *width = (uint32_t) w;
  *height = (uint32_t) h;
}

static const float* desktop_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static void desktop_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void desktop_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void desktop_getBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* desktop_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool desktop_getPose(Device device, vec3 position, quat orientation) {
  if (device == DEVICE_HEAD) {
    vec3_set(position, 0.f, 0.f, 0.f);
    mat4_transform(state.headTransform, position);
    quat_fromMat4(orientation, state.headTransform);
    return true;
  } else if ( device == DEVICE_HAND_LEFT ) {
    mat4_getPosition(state.leftHandTransform, position);
    quat_fromMat4(orientation, state.leftHandTransform);
    return true;
  }
  return false;
}

static bool desktop_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  if (device != DEVICE_HEAD) {
    return false;
  }

  vec3_init(velocity, state.velocity);
  vec3_init(angularVelocity, state.angularVelocity);
  return true;
}

static bool desktop_isDown(Device device, DeviceButton button, bool* down) {
  if (device != DEVICE_HAND_LEFT || button != BUTTON_TRIGGER) {
    return false;
  }

  *down = lovrPlatformIsMouseDown(MOUSE_RIGHT);
  return true;
}

static bool desktop_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool desktop_getAxis(Device device, DeviceAxis axis, vec3 value) {
  return false;
}

static bool desktop_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static ModelData* desktop_newModelData(Device device) {
  return NULL;
}

static void desktop_renderTo(void (*callback)(void*), void* userdata) {
  uint32_t width, height;
  desktop_getDisplayDimensions(&width, &height);
  Camera camera = { .canvas = NULL, .viewMatrix = { MAT4_IDENTITY }, .stereo = true };
  mat4_perspective(camera.projection[0], state.clipNear, state.clipFar, 67.f * (float) M_PI / 180.f, (float) width / 2.f / height);
  mat4_multiply(camera.viewMatrix[0], state.headTransform);
  mat4_invertPose(camera.viewMatrix[0]);
  mat4_set(camera.projection[1], camera.projection[0]);
  mat4_set(camera.viewMatrix[1], camera.viewMatrix[0]);
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static void desktop_update(float dt) {
  bool front = lovrPlatformIsKeyDown(KEY_W) || lovrPlatformIsKeyDown(KEY_UP);
  bool back = lovrPlatformIsKeyDown(KEY_S) || lovrPlatformIsKeyDown(KEY_DOWN);
  bool left = lovrPlatformIsKeyDown(KEY_A) || lovrPlatformIsKeyDown(KEY_LEFT);
  bool right = lovrPlatformIsKeyDown(KEY_D) || lovrPlatformIsKeyDown(KEY_RIGHT);
  bool up = lovrPlatformIsKeyDown(KEY_Q);
  bool down = lovrPlatformIsKeyDown(KEY_E);

  float movespeed = 3.f * dt;
  float turnspeed = 3.f * dt;
  float damping = MAX(1.f - 20.f * dt, 0);
  
  int width, height;
  double mx, my, aspect = 1;
  lovrPlatformGetWindowSize(&width, &height);
  lovrPlatformGetMousePosition(&mx, &my);
  
  // Mouse move
  if (lovrPlatformIsMouseDown(MOUSE_LEFT)) {
    lovrPlatformSetMouseMode(MOUSE_MODE_GRABBED);

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
  state.localVelocity[3] = 0.f;
  vec3_init(state.velocity, state.localVelocity);
  mat4_transformDirection(state.headTransform, state.velocity);
  vec3_scale(state.localVelocity, damping);

  // Update position
  vec3_add(state.position, state.velocity);

  // Update orientation
  state.pitch = CLAMP(state.pitch - state.angularVelocity[0] * turnspeed, -(float) M_PI / 2.f, (float) M_PI / 2.f);
  state.yaw -= state.angularVelocity[1] * turnspeed;

  // Update head transform
  mat4_identity(state.headTransform);
  mat4_translate(state.headTransform, 0.f, state.offset, 0.f);
  mat4_translate(state.headTransform, state.position[0], state.position[1], state.position[2]);
  mat4_rotate(state.headTransform, state.yaw, 0.f, 1.f, 0.f);
  mat4_rotate(state.headTransform, state.pitch, 1.f, 0.f, 0.f);
  
  // Update hand transform to follow cursor
  double px = mx, py = my;
  if (width > 0 && height > 0) {
    // change coordinate system to -1.0 to 1.0
    px = (px / width)*2 - 1.0;
    py = (py / height)*2 - 1.0;
    aspect = height/(double)width;
    
    px +=  0.2; // neutral position = pointing towards center-ish
    px *= 0.6; // fudged range to juuust cover pointing at the whole scene, but not outside it
  }

  mat4_set(state.leftHandTransform, state.headTransform);
  double xrange = M_PI*0.2;
  double yrange = xrange * aspect;
  mat4_translate(state.leftHandTransform, -.1f, -.1f, -0.10f);
  mat4_rotate(state.leftHandTransform, -px * xrange, 0, 1, 0);
  mat4_rotate(state.leftHandTransform, -py * yrange, 1, 0, 0);
  mat4_translate(state.leftHandTransform, 0, 0, -.20f);
  mat4_rotate(state.leftHandTransform, -px * xrange, 0, 1, 0);
  mat4_rotate(state.leftHandTransform, -py * yrange, 1, 0, 0);
}

HeadsetInterface lovrHeadsetDesktopDriver = {
  .driverType = DRIVER_DESKTOP,
  .init = desktop_init,
  .destroy = desktop_destroy,
  .getName = desktop_getName,
  .getOriginType = desktop_getOriginType,
  .getDisplayTime = desktop_getDisplayTime,
  .getDisplayDimensions = desktop_getDisplayDimensions,
  .getDisplayMask = desktop_getDisplayMask,
  .getClipDistance = desktop_getClipDistance,
  .setClipDistance = desktop_setClipDistance,
  .getBoundsDimensions = desktop_getBoundsDimensions,
  .getBoundsGeometry = desktop_getBoundsGeometry,
  .getPose = desktop_getPose,
  .getVelocity = desktop_getVelocity,
  .isDown = desktop_isDown,
  .isTouched = desktop_isTouched,
  .getAxis = desktop_getAxis,
  .vibrate = desktop_vibrate,
  .newModelData = desktop_newModelData,
  .renderTo = desktop_renderTo,
  .update = desktop_update
};
