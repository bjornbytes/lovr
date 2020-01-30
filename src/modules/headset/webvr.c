#include "headset/headset.h"
#include "graphics/graphics.h"
#include <stdbool.h>
#include <stdint.h>

// Provided by resources/webvr.js
extern bool webvr_init(float offset, uint32_t msaa);
extern void webvr_destroy(void);
extern bool webvr_getName(char* name, size_t length);
extern HeadsetOrigin webvr_getOriginType(void);
extern double webvr_getDisplayTime(void);
extern void webvr_getDisplayDimensions(uint32_t* width, uint32_t* height);
extern const float* webvr_getDisplayMask(uint32_t* count);
extern uint32_t webvr_getViewCount(void);
extern bool webvr_getViewPose(uint32_t view, float* position, float* orientation);
extern bool webvr_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down);
extern void webvr_getClipDistance(float* near, float* far);
extern void webvr_setClipDistance(float near, float far);
extern void webvr_getBoundsDimensions(float* width, float* depth);
extern const float* webvr_getBoundsGeometry(uint32_t* count);
extern bool webvr_getPose(Device device, float* position, float* orientation);
extern bool webvr_getVelocity(Device device, float* velocity, float* angularVelocity);
extern bool webvr_isDown(Device device, DeviceButton button, bool* down, bool* changed);
extern bool webvr_isTouched(Device device, DeviceButton button, bool* touched);
extern bool webvr_getAxis(Device device, DeviceAxis axis, float* value);
extern bool webvr_vibrate(Device device, float strength, float duration, float frequency);
extern struct ModelData* webvr_newModelData(Device device);
extern void webvr_update(float dt);

static struct {
  void (*renderCallback)(void*);
  void* renderData;
} state;

void webvr_onAnimationFrame(float* leftView, float* rightView, float* leftProjection, float* rightProjection) {
  Camera camera = { .canvas = NULL, .stereo = true };
  memcpy(camera.projection[0], leftProjection, 16 * sizeof(float));
  memcpy(camera.projection[1], rightProjection, 16 * sizeof(float));
  memcpy(camera.viewMatrix[0], leftView, 16 * sizeof(float));
  memcpy(camera.viewMatrix[1], rightView, 16 * sizeof(float));
  lovrGraphicsSetCamera(&camera, true);
  state.renderCallback(state.renderData);
  lovrGraphicsSetCamera(NULL, false);
}

void webvr_renderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  state.renderData = userdata;
}

HeadsetInterface lovrHeadsetWebVRDriver = {
  .driverType = DRIVER_WEBVR,
  .init = webvr_init,
  .destroy = webvr_destroy,
  .getName = webvr_getName,
  .getOriginType = webvr_getOriginType,
  .getDisplayTime = webvr_getDisplayTime,
  .getDisplayDimensions = webvr_getDisplayDimensions,
  .getDisplayMask = webvr_getDisplayMask,
  .getViewCount = webvr_getViewCount,
  .getViewPose = webvr_getViewPose,
  .getViewAngles = webvr_getViewAngles,
  .getClipDistance = webvr_getClipDistance,
  .setClipDistance = webvr_setClipDistance,
  .getBoundsDimensions = webvr_getBoundsDimensions,
  .getBoundsGeometry = webvr_getBoundsGeometry,
  .getPose = webvr_getPose,
  .getVelocity = webvr_getVelocity,
  .isDown = webvr_isDown,
  .isTouched = webvr_isTouched,
  .getAxis = webvr_getAxis,
  .vibrate = webvr_vibrate,
  .newModelData = webvr_newModelData,
  .renderTo = webvr_renderTo,
  .update = webvr_update
};
