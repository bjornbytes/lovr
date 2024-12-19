#include "headset/headset.h"
#include "data/image.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "system/system.h"
#include "core/maf.h"
#include "core/os.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MOVESPEED 3.f
#define SPRINTSPEED 15.f
#define SLOWSPEED .5f
#define MOVESMOOTH 30.f
#define TURNSPEED .005f
#define TURNSMOOTH 30.f
#define OFFSET (state.config.seated ? 0.f : 1.7f)

struct Layer {
  uint32_t ref;
  uint32_t textureWidth;
  uint32_t textureWeight;
  float position[3];
  float orientation[4];
  float width;
  float height;
  float curve;
  int32_t viewport[4];
  Texture* texture;
  Pass* pass;
};

static struct {
  bool initialized;
  bool active;
  HeadsetConfig config;
  TextureFormat depthFormat;
  Texture* texture;
  Pass* pass;
  Layer* layers[MAX_LAYERS];
  uint32_t layerCount;
  bool showMainLayer;
  float pitch;
  float yaw;
  float distance;
  float velocity[3];
  float headPosition[3];
  float headOrientation[4];
  float handPosition[3];
  float handOrientation[4];
  double epoch;
  double time;
  double dt;
  double mx;
  double my;
  double mxHand;
  double myHand;
  bool triggerDown;
  bool triggerChanged;
  bool focused;
  float clipNear;
  float clipFar;
} state;

static void onFocus(bool focused) {
  state.focused = focused;
  lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { focused } });
}

static bool simulator_init(HeadsetConfig* config) {
  state.config = *config;
  state.clipNear = .01f;
  state.clipFar = 0.f;
  state.distance = .5f;

  if (!state.initialized) {
    vec3_set(state.headPosition, 0.f, 0.f, 0.f);
    vec3_set(state.handPosition, 0.f, 0.f, 0.f);
    quat_identity(state.headOrientation);
    quat_identity(state.handOrientation);
    state.initialized = true;
  }

  state.focused = true;
  os_on_focus(onFocus);

  return true;
}

static bool simulator_start(void) {
  if (state.active) {
    return true;
  }

#ifdef LOVR_DISABLE_GRAPHICS
  bool hasGraphics = false;
#else
  bool hasGraphics = lovrGraphicsIsInitialized();
#endif

  if (hasGraphics) {
    state.pass = lovrPassCreate("Headset");
    if (!state.pass) return false;
    state.depthFormat = state.config.stencil ? FORMAT_D32FS8 : FORMAT_D32F;
    if (!lovrGraphicsGetFormatSupport(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = state.config.stencil ? FORMAT_D24S8 : FORMAT_D24;
    }
  }

  state.epoch = os_get_time();
  state.time = 0.;
  state.active = true;
  state.showMainLayer = true;
  return true;
}

static void simulator_stop(void) {
  if (!state.active) return;
  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
  }
  lovrRelease(state.texture, lovrTextureDestroy);
  lovrRelease(state.pass, lovrPassDestroy);
  state.texture = NULL;
  state.pass = NULL;
  state.active = false;
}

static void simulator_destroy(void) {
  simulator_stop();
}

static bool simulator_getDriverName(char* name, size_t length) {
  strncpy(name, "LÖVR", length - 1);
  name[length - 1] = '\0';
  return true;
}

static void simulator_getFeatures(HeadsetFeatures* features) {
  features->overlay = false;
  features->proximity = false;
  features->passthrough = false;
  features->refreshRate = false;
  features->depthSubmission = false;
  features->eyeTracking = false;
  features->handTracking = false;
  features->handTrackingElbow = false;
  features->keyboardTracking = false;
  features->viveTrackers = false;
  features->handModel = false;
  features->controllerModel = false;
  features->controllerSkeleton = false;
  features->layerCube = false;
  features->layerSphere = false;
  features->layerCurve = false;
  features->layerDepthTest = false;
  features->layerFilter = false;
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
  float density = os_window_get_pixel_density();
  os_window_get_size(width, height);
  *width *= state.config.supersample;
  *height *= state.config.supersample;
  *width *= density;
  *height *= density;
}

static float simulator_getRefreshRate(void) {
  return 0.f;
}

static bool simulator_setRefreshRate(float refreshRate) {
  return lovrSetError("Device does not support setting refresh rate");
}

static const float* simulator_getRefreshRates(uint32_t* count) {
  return *count = 0, NULL;
}

static void simulator_getFoveation(FoveationLevel* level, bool* dynamic) {
  *level = FOVEATION_NONE;
  *dynamic = false;
}

static bool simulator_setFoveation(FoveationLevel level, bool dynamic) {
  return level == FOVEATION_NONE;
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
  return state.time;
}

static double simulator_getDeltaTime(void) {
  return state.dt;
}

static uint32_t simulator_getViewCount(void) {
  return 1;
}

static bool simulator_getViewPose(uint32_t view, float* position, float* orientation) {
  vec3_init(position, state.headPosition);
  quat_init(orientation, state.headOrientation);
  position[1] += OFFSET;
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
    vec3_init(position, state.headPosition);
    quat_init(orientation, state.headOrientation);
    position[1] += OFFSET;
    return true;
  } else if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_LEFT_POINT) {
    vec3_init(position, state.handPosition);
    quat_init(orientation, state.handOrientation);
    return true;
  }
  return false;
}

static bool simulator_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  vec3_init(velocity, state.velocity);
  vec3_set(angularVelocity, 0.f, 0.f, 0.f);
  return device == DEVICE_HEAD;
}

static bool simulator_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  *down = state.triggerDown;
  *changed = state.triggerChanged;
  return device == DEVICE_HAND_LEFT && button == BUTTON_TRIGGER;
}

static bool simulator_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool simulator_getAxis(Device device, DeviceAxis axis, vec3 value) {
  return false;
}

static bool simulator_getSkeleton(Device device, float* poses, SkeletonSource* source) {
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

static Texture* simulator_setBackground(uint32_t width, uint32_t height, uint32_t layers) {
  lovrSetError("NYI");
  return NULL;
}

static Layer* simulator_newLayer(const LayerInfo* info) {
  Layer* layer = lovrCalloc(sizeof(Layer));
  layer->ref = 1;
  layer->textureWidth = info->width;
  layer->textureWeight = info->height;
  layer->texture = lovrTextureCreate(&(TextureInfo) {
    .format = FORMAT_RGBA8,
    .type = info->stereo ? TEXTURE_ARRAY : TEXTURE_2D,
    .width = info->width,
    .height = info->height,
    .layers = 1 << info->stereo,
    .usage = TEXTURE_RENDER | TEXTURE_TRANSFER,
    .srgb = true
  });
  layer->pass = lovrPassCreate("Layer");
  return layer;
}

static void simulator_destroyLayer(void* ref) {
  Layer* layer = ref;
  lovrRelease(layer->texture, lovrTextureDestroy);
  lovrRelease(layer->pass, lovrPassDestroy);
  lovrFree(layer);
}

static Layer** simulator_getLayers(uint32_t* count, bool* main) {
  *count = state.layerCount;
  *main = state.showMainLayer;
  return state.layers;
}

static bool simulator_setLayers(Layer** layers, uint32_t count, bool main) {
  lovrCheck(count <= MAX_LAYERS, "Too many layers");

  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
  }

  state.layerCount = count;
  for (uint32_t i = 0; i < count; i++) {
    lovrRetain(layers[i]);
    state.layers[i] = layers[i];
  }

  state.showMainLayer = main;

  return true;
}

static void simulator_getLayerPose(Layer* layer, float* position, float* orientation) {
  memcpy(position, layer->position, 3 * sizeof(float));
  memcpy(orientation, layer->orientation, 4 * sizeof(float));
}

static void simulator_setLayerPose(Layer* layer, float* position, float* orientation) {
  memcpy(layer->position, position, 3 * sizeof(float));
  memcpy(layer->orientation, orientation, 4 * sizeof(float));
}

static void simulator_getLayerDimensions(Layer* layer, float* width, float* height) {
  *width = layer->width;
  *height = layer->height;
}

static void simulator_setLayerDimensions(Layer* layer, float width, float height) {
  layer->width = width;
  layer->height = height;
}

static void simulator_getLayerColor(Layer* layer, float color[4]) {
  color[0] = 1.f;
  color[1] = 1.f;
  color[2] = 1.f;
  color[3] = 1.f;
}

static void simulator_setLayerColor(Layer* layer, float color[4]) {
  //
}

static float simulator_getLayerCurve(Layer* layer) {
  return layer->curve;
}

static bool simulator_setLayerCurve(Layer* layer, float curve) {
  layer->curve = curve;
  return true;
}

static void simulator_getLayerViewport(Layer* layer, int32_t viewport[4]) {
  memcpy(viewport, layer->viewport, 4 * sizeof(int32_t));
}

static void simulator_setLayerViewport(Layer* layer, int32_t viewport[4]) {
  memcpy(layer->viewport, viewport, 4 * sizeof(int32_t));
  if (layer->viewport[2] == 0) layer->viewport[2] = layer->width - layer->viewport[0];
  if (layer->viewport[3] == 0) layer->viewport[3] = layer->height - layer->viewport[1];
}

static struct Texture* simulator_getLayerTexture(Layer* layer) {
  return layer->texture;
}

static struct Pass* simulator_getLayerPass(Layer* layer) {
  return layer->pass;
}

static bool simulator_getTexture(Texture** texture) {
  *texture = state.texture;
  return true;
}

static bool simulator_getPass(Pass** pass) {
  if (!state.pass || !os_window_is_open()) {
    *pass = NULL;
    return true;
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
      .usage = TEXTURE_RENDER | TEXTURE_SAMPLE
    });

    if (!state.texture) {
      return false;
    }

    CanvasTexture color[4] = { [0].texture = state.texture };
    if (!lovrPassSetCanvas(state.pass, color, NULL, state.depthFormat, NULL, state.config.antialias ? 4 : 1)) {
      return false;
    }
  }

  float background[4][4];
  LoadAction loads[4] = { LOAD_CLEAR };
  lovrGraphicsGetBackgroundColor(background[0]);
  lovrPassSetClear(state.pass, loads, background, LOAD_CLEAR, 0.f);

  float viewMatrix[16];
  mat4_fromPose(viewMatrix, state.headPosition, state.headOrientation);
  viewMatrix[13] += OFFSET;
  mat4_invert(viewMatrix);

  float projection[16];
  float left, right, up, down;
  simulator_getViewAngles(0, &left, &right, &up, &down);
  mat4_fov(projection, left, right, up, down, state.clipNear, state.clipFar);

  lovrPassSetViewMatrix(state.pass, 0, viewMatrix);
  lovrPassSetProjection(state.pass, 0, projection);

  *pass = state.pass;
  return true;
}

static bool simulator_submit(void) {
  return true;
}

static bool simulator_isActive(void) {
  return state.active;
}

static bool simulator_isVisible(void) {
  return true;
}

static bool simulator_isFocused(void) {
  return state.focused;
}

static bool simulator_isMounted(void) {
  return true;
}

static bool simulator_update(double* dt) {
  if (!state.active) {
    *dt = 0.;
    return true;
  }

  double t = os_get_time() - state.epoch;
  state.dt = t - state.time;
  state.time = t;

  bool trigger = os_is_mouse_down(MOUSE_RIGHT);
  state.triggerChanged = trigger != state.triggerDown;
  state.triggerDown = trigger;

  bool click = os_is_mouse_down(MOUSE_LEFT);
  os_set_mouse_mode(click ? MOUSE_MODE_GRABBED : MOUSE_MODE_NORMAL);

  double mxprev = state.mx;
  double myprev = state.my;

  os_get_mouse_position(&state.mx, &state.my);

  if (click) {
    state.pitch = CLAMP(state.pitch - (state.my - myprev) * TURNSPEED, -(float) M_PI / 2.f, (float) M_PI / 2.f);
    state.yaw -= (state.mx - mxprev) * TURNSPEED;
  } else {
    state.mxHand = state.mx * state.config.supersample;
    state.myHand = state.my * state.config.supersample;
  }

  // Head

  float pitch[4], yaw[4], target[4];
  quat_fromAngleAxis(pitch, state.pitch, 1.f, 0.f, 0.f);
  quat_fromAngleAxis(yaw, state.yaw, 0.f, 1.f, 0.f);
  quat_mul(target, yaw, pitch);
  quat_slerp(state.headOrientation, target, 1.f - expf(-TURNSMOOTH * state.dt));

  bool sprint = os_is_key_down(OS_KEY_LEFT_SHIFT) || os_is_key_down(OS_KEY_RIGHT_SHIFT);
  bool slow = os_is_key_down(OS_KEY_LEFT_CONTROL) || os_is_key_down(OS_KEY_RIGHT_CONTROL);
  bool front = os_is_key_down(OS_KEY_W) || os_is_key_down(OS_KEY_UP);
  bool back = os_is_key_down(OS_KEY_S) || os_is_key_down(OS_KEY_DOWN);
  bool left = os_is_key_down(OS_KEY_A) || os_is_key_down(OS_KEY_LEFT);
  bool right = os_is_key_down(OS_KEY_D) || os_is_key_down(OS_KEY_RIGHT);
  bool up = os_is_key_down(OS_KEY_Q);
  bool down = os_is_key_down(OS_KEY_E);

  float velocity[3];
  velocity[0] = (left ? -1.f : right ? 1.f : 0.f);
  velocity[1] = (down ? -1.f : up ? 1.f : 0.f);
  velocity[2] = (front ? -1.f : back ? 1.f : 0.f);
  vec3_scale(velocity, sprint ? SPRINTSPEED : (slow ? SLOWSPEED : MOVESPEED));
  vec3_lerp(state.velocity, velocity, 1.f - expf(-MOVESMOOTH * state.dt));

  vec3_scale(vec3_init(velocity, state.velocity), state.dt);
  quat_rotate(state.headOrientation, velocity);
  vec3_add(state.headPosition, velocity);

  // Hand

  float inverseProjection[16], angleLeft, angleRight, angleUp, angleDown;
  simulator_getViewAngles(0, &angleLeft, &angleRight, &angleUp, &angleDown);
  mat4_fov(inverseProjection, angleLeft, angleRight, angleUp, angleDown, state.clipNear, state.clipFar);
  mat4_invert(inverseProjection);

  float ray[3];
  uint32_t width, height;
  os_window_get_size(&width, &height);
  width *= state.config.supersample;
  height *= state.config.supersample;
  vec3_set(ray, state.mxHand / width * 2.f - 1.f, state.myHand / height * 2.f - 1.f, 1.f);

  mat4_mulPoint(inverseProjection, ray);
  quat_rotate(state.headOrientation, ray);
  vec3_normalize(ray);

  state.distance = CLAMP(state.distance * (1.f + lovrSystemGetScrollDelta() * .05f), .05f, 10.f);

  vec3_init(state.handPosition, ray);
  vec3_scale(state.handPosition, state.distance);
  vec3_add(state.handPosition, state.headPosition);
  state.handPosition[1] += OFFSET;

  float zero[3], y[3], basis[16];
  vec3_set(zero, 0.f, 0.f, 0.f);
  vec3_set(y, 0.f, 1.f, 0.f);
  quat_rotate(state.headOrientation, y);
  mat4_target(basis, zero, ray, y);
  quat_fromMat4(state.handOrientation, basis);

  *dt = state.dt;
  return true;
}

HeadsetInterface lovrHeadsetSimulatorDriver = {
  .driverType = DRIVER_SIMULATOR,
  .init = simulator_init,
  .start = simulator_start,
  .stop = simulator_stop,
  .destroy = simulator_destroy,
  .getDriverName = simulator_getDriverName,
  .getFeatures = simulator_getFeatures,
  .getName = simulator_getName,
  .isSeated = simulator_isSeated,
  .getDisplayDimensions = simulator_getDisplayDimensions,
  .getRefreshRate = simulator_getRefreshRate,
  .setRefreshRate = simulator_setRefreshRate,
  .getRefreshRates = simulator_getRefreshRates,
  .getFoveation = simulator_getFoveation,
  .setFoveation = simulator_setFoveation,
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
  .setBackground = simulator_setBackground,
  .newLayer = simulator_newLayer,
  .destroyLayer = simulator_destroyLayer,
  .getLayers = simulator_getLayers,
  .setLayers = simulator_setLayers,
  .getLayerPose = simulator_getLayerPose,
  .setLayerPose = simulator_setLayerPose,
  .getLayerDimensions = simulator_getLayerDimensions,
  .setLayerDimensions = simulator_setLayerDimensions,
  .getLayerCurve = simulator_getLayerCurve,
  .setLayerCurve = simulator_setLayerCurve,
  .getLayerColor = simulator_getLayerColor,
  .setLayerColor = simulator_setLayerColor,
  .getLayerViewport = simulator_getLayerViewport,
  .setLayerViewport = simulator_setLayerViewport,
  .getLayerTexture = simulator_getLayerTexture,
  .getLayerPass = simulator_getLayerPass,
  .getTexture = simulator_getTexture,
  .getPass = simulator_getPass,
  .submit = simulator_submit,
  .isActive = simulator_isActive,
  .isVisible = simulator_isVisible,
  .isFocused = simulator_isFocused,
  .isMounted = simulator_isMounted,
  .update = simulator_update
};
