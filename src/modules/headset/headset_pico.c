#include "headset/headset.h"
#include <string.h>

static struct {
  float offset;
} state;

static bool pico_init(float offset, uint32_t msaa) {
  state.offset = offset;
  return true;
}

static void pico_destroy(void) {
  memset(&state, 0, sizeof(state));
}

static bool pico_getName(char* name, size_t length) {
  return false;
}

static HeadsetOrigin pico_getOriginType(void) {
  return ORIGIN_HEAD;
}

static double pico_getDisplayTime(void) {
  return 0.;
}

static void pico_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = 0;
  *height = 0;
}

static const float* pico_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static uint32_t pico_getViewCount(void) {
  return 2;
}

static bool pico_getViewPose(uint32_t view, float* position, float* orientation) {
  return false;
}

static bool pico_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  return false;
}

static void pico_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = 0.f;
  *clipFar = 0.f;
}

static void pico_setClipDistance(float clipNear, float clipFar) {
  //
}

static void pico_getBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* pico_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool pico_getPose(Device device, float* position, float* orientation) {
  return false;
}

static bool pico_getVelocity(Device device, float* velocity, float* angularVelocity) {
  return false;
}

static bool pico_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  return false;
}

static bool pico_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool pico_getAxis(Device device, DeviceAxis axis, float* value) {
  return false;
}

static bool pico_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static struct ModelData* pico_newModelData(Device device) {
  return NULL;
}

static void pico_renderTo(void (*callback)(void*), void* userdata) {
  //
}

static void pico_update(float dt) {
  //
}

HeadsetInterface lovrHeadsetPicoDriver = {
  .driverType = DRIVER_PICO,
  .init = pico_init,
  .destroy = pico_destroy,
  .getName = pico_getName,
  .getOriginType = pico_getOriginType,
  .getDisplayTime = pico_getDisplayTime,
  .getDisplayDimensions = pico_getDisplayDimensions,
  .getDisplayMask = pico_getDisplayMask,
  .getViewCount = pico_getViewCount,
  .getViewPose = pico_getViewPose,
  .getViewAngles = pico_getViewAngles,
  .getClipDistance = pico_getClipDistance,
  .setClipDistance = pico_setClipDistance,
  .getBoundsDimensions = pico_getBoundsDimensions,
  .getBoundsGeometry = pico_getBoundsGeometry,
  .getPose = pico_getPose,
  .getVelocity = pico_getVelocity,
  .isDown = pico_isDown,
  .isTouched = pico_isTouched,
  .getAxis = pico_getAxis,
  .vibrate = pico_vibrate,
  .newModelData = pico_newModelData,
  .renderTo = pico_renderTo,
  .update = pico_update
};
