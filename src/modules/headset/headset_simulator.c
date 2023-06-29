#include "headset/headset.h"
#include "data/image.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/os.h"
#include "util.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

static struct {
  bool initialized;
  HeadsetConfig config;
  TextureFormat depthFormat;
  Texture* texture;
  Pass* pass;
  float position[4];
  float velocity[4];
  float localVelocity[4];
  float angularVelocity[4];
  float headTransform[16];
  float leftHandTransform[16];
  double epoch;
  double prevDisplayTime;
  double nextDisplayTime;
  double prevCursorX;
  double prevCursorY;
  bool mouseDown;
  bool prevMouseDown;
  bool focused;
  float clipNear;
  float clipFar;
  float pitch;
  float yaw;
} state;

static void onFocus(bool focused) {
  state.focused = focused;
  lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { focused } });
}

static bool simulator_init(HeadsetConfig* config) {
  state.config = *config;
  state.clipNear = .01f;
  state.clipFar = 0.f;
  state.epoch = os_get_time();
  state.prevDisplayTime = state.epoch;
  state.nextDisplayTime = state.epoch;

  if (!state.initialized) {
    mat4_identity(state.headTransform);
    mat4_identity(state.leftHandTransform);
    state.initialized = true;
  }

  state.focused = true;
  os_on_focus(onFocus);

  return true;
}

static void simulator_start(void) {
#ifdef LOVR_DISABLE_GRAPHICS
  bool hasGraphics = false;
#else
  bool hasGraphics = lovrGraphicsIsInitialized();
#endif

  if (hasGraphics) {
    state.pass = lovrPassCreate();

    state.depthFormat = state.config.stencil ? FORMAT_D32FS8 : FORMAT_D32F;
    if (state.config.stencil && !lovrGraphicsIsFormatSupported(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = FORMAT_D24S8; // Guaranteed to be supported if the other one isn't
    }
  }
}

static void simulator_stop(void) {
  lovrRelease(state.texture, lovrTextureDestroy);
  lovrRelease(state.pass, lovrPassDestroy);
  state.texture = NULL;
  state.pass = NULL;
}

static void simulator_destroy(void) {
  simulator_stop();
}

static bool simulator_getName(char* name, size_t length) {
  strncpy(name, "Simulator", length - 1);
  name[length - 1] = '\0';
  return true;
}

static bool simulator_isSeated(void) {
  return state.config.seated;
}

static void simulator_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  os_window_get_size(width, height);
}

static float simulator_getRefreshRate(void) {
  return 0.f;
}

static bool simulator_setRefreshRate(float refreshRate) {
  return false;
}

static const float* simulator_getRefreshRates(uint32_t* count) {
  return *count = 0, NULL;
}

static PassthroughMode simulator_getPassthrough(void) {
  return PASSTHROUGH_OPAQUE;
}

static bool simulator_setPassthrough(PassthroughMode mode) {
  return mode == PASSTHROUGH_OPAQUE;
}

static bool simulator_isPassthroughSupported(PassthroughMode mode) {
  return mode == PASSTHROUGH_OPAQUE;
}

static double simulator_getDisplayTime(void) {
  return state.nextDisplayTime - state.epoch;
}

static double simulator_getDeltaTime(void) {
  return state.nextDisplayTime - state.prevDisplayTime;
}

static uint32_t simulator_getViewCount(void) {
  return 1;
}

static bool simulator_getViewPose(uint32_t view, float* position, float* orientation) {
  vec3_init(position, state.position);
  quat_fromMat4(orientation, state.headTransform);
  position[1] += state.config.seated ? 0.f : 1.7f;
  return view == 0;
}

static bool simulator_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  float aspect, fov;
  uint32_t width, height;
  simulator_getDisplayDimensions(&width, &height);
  aspect = (float) width / height;
  fov = .7f;
  *left = atanf(tanf(fov) * aspect);
  *right = atanf(tanf(fov) * aspect);
  *up = fov;
  *down = fov;
  return view == 0;
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
  } else if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_LEFT_POINT) {
    mat4_getPosition(state.leftHandTransform, position);
    quat_fromMat4(orientation, state.leftHandTransform);
    return true;
  } else if (device == DEVICE_FLOOR) {
    vec3_set(position, 0.f, state.config.seated ? -1.7f : 0.f, 0.f);
    quat_identity(orientation);
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

static void simulator_stopVibration(Device device) {
  //
}

static struct ModelData* simulator_newModelData(Device device, bool animated) {
  return NULL;
}

static bool simulator_animate(struct Model* model) {
  return false;
}

static Texture* simulator_getTexture(void) {
  return state.texture;
}

static Pass* simulator_getPass(void) {
  if (!state.pass) {
    return NULL;
  }

  lovrPassReset(state.pass);

  uint32_t width, height;
  simulator_getDisplayDimensions(&width, &height);

  if (lovrPassGetWidth(state.pass) != width || lovrPassGetHeight(state.pass) != height) {
    lovrRelease(state.texture, lovrTextureDestroy);

    state.texture = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .format = FORMAT_RGBA8,
      .srgb = true,
      .width = width,
      .height = height,
      .layers = 1,
      .mipmaps = 1,
      .samples = 1,
      .usage = TEXTURE_RENDER | TEXTURE_SAMPLE,
    });

    Texture* textures[4] = { state.texture };
    lovrPassSetCanvas(state.pass, textures, NULL, state.depthFormat, state.config.antialias ? 4 : 1);
  }

  float background[4];
  LoadAction load = LOAD_CLEAR;
  lovrGraphicsGetBackgroundColor(background);
  lovrPassSetClear(state.pass, &load, &background, LOAD_CLEAR, 0.f);

  float position[4], orientation[4];
  simulator_getViewPose(0, position, orientation);

  float viewMatrix[16];
  mat4_fromQuat(viewMatrix, orientation);
  memcpy(viewMatrix + 12, position, 3 * sizeof(float));
  mat4_invert(viewMatrix);

  float projection[16];
  float left, right, up, down;
  simulator_getViewAngles(0, &left, &right, &up, &down);
  mat4_fov(projection, left, right, up, down, state.clipNear, state.clipFar);

  lovrPassSetViewMatrix(state.pass, 0, viewMatrix);
  lovrPassSetProjection(state.pass, 0, projection);

  return state.pass;
}

static void simulator_submit(void) {
  //
}

static bool simulator_isFocused(void) {
  return state.focused;
}

static double simulator_update(void) {
  bool front = os_is_key_down(KEY_W) || os_is_key_down(KEY_UP);
  bool back = os_is_key_down(KEY_S) || os_is_key_down(KEY_DOWN);
  bool left = os_is_key_down(KEY_A) || os_is_key_down(KEY_LEFT);
  bool right = os_is_key_down(KEY_D) || os_is_key_down(KEY_RIGHT);
  bool up = os_is_key_down(KEY_Q);
  bool down = os_is_key_down(KEY_E);

  state.prevDisplayTime = state.nextDisplayTime;
  state.nextDisplayTime = os_get_time();
  double dt = state.nextDisplayTime - state.prevDisplayTime;

  float movespeed = 3.f * (float) dt;
  float turnspeed = 3.f * (float) dt;
  float damping = MAX(1.f - 20.f * (float) dt, 0);

  double mx, my;
  uint32_t width, height;
  os_get_mouse_position(&mx, &my);
  os_window_get_size(&width, &height);

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
    state.angularVelocity[0] = dy / (float) dt;
    state.angularVelocity[1] = dx / (float) dt;
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
  mat4_translate(state.headTransform, 0.f, state.config.seated ? 0.f : 1.7f, 0.f);
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
  return dt;
}

HeadsetInterface lovrHeadsetSimulatorDriver = {
  .driverType = DRIVER_SIMULATOR,
  .init = simulator_init,
  .start = simulator_start,
  .stop = simulator_stop,
  .destroy = simulator_destroy,
  .getName = simulator_getName,
  .isSeated = simulator_isSeated,
  .getDisplayDimensions = simulator_getDisplayDimensions,
  .getRefreshRate = simulator_getRefreshRate,
  .setRefreshRate = simulator_setRefreshRate,
  .getRefreshRates = simulator_getRefreshRates,
  .getPassthrough = simulator_getPassthrough,
  .setPassthrough = simulator_setPassthrough,
  .isPassthroughSupported = simulator_isPassthroughSupported,
  .getDisplayTime = simulator_getDisplayTime,
  .getDeltaTime = simulator_getDeltaTime,
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
  .stopVibration = simulator_stopVibration,
  .newModelData = simulator_newModelData,
  .animate = simulator_animate,
  .getTexture = simulator_getTexture,
  .getPass = simulator_getPass,
  .submit = simulator_submit,
  .isFocused = simulator_isFocused,
  .update = simulator_update
};
