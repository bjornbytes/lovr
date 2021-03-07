#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

static void onFocus(bool focused) {
  lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { focused } });
}

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
  bool mouseDown;
  bool prevMouseDown;

  float offset;
  float clipNear;
  float clipFar;
  float pitch;
  float yaw;
} state;

static bool simulator_init(float supersample, float offset, uint32_t msaa) {
  state.offset = offset;
  state.clipNear = .1f;
  state.clipFar = 100.f;

  if (!state.initialized) {
    mat4_identity(state.headTransform);
    mat4_identity(state.leftHandTransform);
    state.initialized = true;
  }

  os_on_focus(onFocus);

  return true;
}

static void simulator_destroy(void) {
  //
}

static bool simulator_getName(char* name, size_t length) {
  strncpy(name, "Simulator", length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin simulator_getOriginType(void) {
  return ORIGIN_HEAD;
}

static double simulator_getDisplayTime(void) {
  return os_get_time();
}

static void simulator_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  int w, h;
  os_window_get_fbsize(&w, &h);
  *width = (uint32_t) w / 2;
  *height = (uint32_t) h;
}

static const float* simulator_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static uint32_t simulator_getViewCount(void) {
  return 2;
}

static bool simulator_getViewPose(uint32_t view, float* position, float* orientation) {
  vec3_init(position, state.position);
  quat_fromMat4(orientation, state.headTransform);
  position[1] += state.offset;
  return view < 2;
}

static bool simulator_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  float aspect, fov;
  uint32_t width, height;
  simulator_getDisplayDimensions(&width, &height);
  aspect = (float) width / height;
  fov = 67.f * (float) M_PI / 180.f * .5f;
  *left = fov * aspect;
  *right = fov * aspect;
  *up = fov;
  *down = fov;
  return view < 2;
}

static void simulator_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void simulator_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void simulator_getBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* simulator_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool simulator_getPose(Device device, vec3 position, quat orientation) {
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

static bool simulator_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  if (device != DEVICE_HEAD) {
    return false;
  }

  vec3_init(velocity, state.velocity);
  vec3_init(angularVelocity, state.angularVelocity);
  return true;
}

static bool simulator_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (device != DEVICE_HAND_LEFT || button != BUTTON_TRIGGER) {
    return false;
  }
  *down = state.mouseDown;
  *changed = state.mouseDown != state.prevMouseDown;
  return true;
}

static bool simulator_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool simulator_getAxis(Device device, DeviceAxis axis, vec3 value) {
  return false;
}

static bool simulator_getSkeleton(Device device, float* poses) {
  return false;
}

static bool simulator_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static ModelData* simulator_newModelData(Device device, bool animated) {
  return NULL;
}

static bool simulator_animate(Device device, struct Model* model) {
  return false;
}

static void simulator_renderTo(void (*callback)(void*), void* userdata) {
  float projection[16], left, right, up, down;
  simulator_getViewAngles(0, &left, &right, &up, &down);
  mat4_fov(projection, left, right, up, down, state.clipNear, state.clipFar);

  float viewMatrix[16];
  mat4_invert(mat4_init(viewMatrix, state.headTransform));

  lovrGraphicsSetProjection(0, projection);
  lovrGraphicsSetProjection(1, projection);
  lovrGraphicsSetViewMatrix(0, viewMatrix);
  lovrGraphicsSetViewMatrix(1, viewMatrix);
  lovrGraphicsSetBackbuffer(NULL, true, true);
  callback(userdata);
  lovrGraphicsSetBackbuffer(NULL, false, false);
}

static void simulator_update(float dt) {
  bool front = os_is_key_down(KEY_W) || os_is_key_down(KEY_UP);
  bool back = os_is_key_down(KEY_S) || os_is_key_down(KEY_DOWN);
  bool left = os_is_key_down(KEY_A) || os_is_key_down(KEY_LEFT);
  bool right = os_is_key_down(KEY_D) || os_is_key_down(KEY_RIGHT);
  bool up = os_is_key_down(KEY_Q);
  bool down = os_is_key_down(KEY_E);

  float movespeed = 3.f * dt;
  float turnspeed = 3.f * dt;
  float damping = MAX(1.f - 20.f * dt, 0);

  int width, height;
  double mx, my;
  os_window_get_size(&width, &height);
  os_get_mouse_position(&mx, &my);

  double aspect = (width > 0 && height > 0) ? ((double) width / height) : 1.;

  // Mouse move
  if (os_is_mouse_down(MOUSE_LEFT)) {
    os_set_mouse_mode(MOUSE_MODE_GRABBED);

    if (state.prevCursorX == -1 && state.prevCursorY == -1) {
      state.prevCursorX = mx;
      state.prevCursorY = my;
    }

    float dx = (float) (mx - state.prevCursorX) / ((float) width);
    float dy = (float) (my - state.prevCursorY) / ((float) height * aspect);
    state.angularVelocity[0] = dy / dt;
    state.angularVelocity[1] = dx / dt;
    state.prevCursorX = mx;
    state.prevCursorY = my;
  } else {
    os_set_mouse_mode(MOUSE_MODE_NORMAL);
    vec3_scale(state.angularVelocity, damping);
    state.prevCursorX = state.prevCursorY = -1;
  }

  state.prevMouseDown = state.mouseDown;
  state.mouseDown = os_is_mouse_down(MOUSE_RIGHT);

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
    px = (px / width) * 2 - 1.0;
    py = (py / height) * 2 - 1.0;

    px +=  .2; // neutral position = pointing towards center-ish
    px *= .6; // fudged range to juuust cover pointing at the whole scene, but not outside it
  }

  mat4_set(state.leftHandTransform, state.headTransform);
  double xrange = M_PI * .2;
  double yrange = xrange / aspect;
  mat4_translate(state.leftHandTransform, -.1f, -.1f, -0.10f);
  mat4_rotate(state.leftHandTransform, -px * xrange, 0, 1, 0);
  mat4_rotate(state.leftHandTransform, -py * yrange, 1, 0, 0);
  mat4_translate(state.leftHandTransform, 0, 0, -.20f);
  mat4_rotate(state.leftHandTransform, -px * xrange, 0, 1, 0);
  mat4_rotate(state.leftHandTransform, -py * yrange, 1, 0, 0);
}

HeadsetInterface lovrHeadsetSimulatorDriver = {
  .driverType = DRIVER_SIMULATOR,
  .init = simulator_init,
  .destroy = simulator_destroy,
  .getName = simulator_getName,
  .getOriginType = simulator_getOriginType,
  .getDisplayTime = simulator_getDisplayTime,
  .getDisplayDimensions = simulator_getDisplayDimensions,
  .getDisplayMask = simulator_getDisplayMask,
  .getViewCount = simulator_getViewCount,
  .getViewPose = simulator_getViewPose,
  .getViewAngles = simulator_getViewAngles,
  .getClipDistance = simulator_getClipDistance,
  .setClipDistance = simulator_setClipDistance,
  .getBoundsDimensions = simulator_getBoundsDimensions,
  .getBoundsGeometry = simulator_getBoundsGeometry,
  .getPose = simulator_getPose,
  .getVelocity = simulator_getVelocity,
  .isDown = simulator_isDown,
  .isTouched = simulator_isTouched,
  .getAxis = simulator_getAxis,
  .getSkeleton = simulator_getSkeleton,
  .vibrate = simulator_vibrate,
  .newModelData = simulator_newModelData,
  .animate = simulator_animate,
  .renderTo = simulator_renderTo,
  .update = simulator_update
};
