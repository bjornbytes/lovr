#include "headset/headset.h"
#include <stdbool.h>

extern bool webxr_init(float offset, uint32_t msaa);
extern void webxr_destroy(void);
extern bool webxr_getName(char* name, size_t length);
extern HeadsetOrigin webxr_getOriginType(void);
extern double webxr_getDisplayTime(void);
extern void webxr_getDisplayDimensions(uint32_t* width, uint32_t* height);
extern const float* webxr_getDisplayMask(uint32_t* count);
extern uint32_t webxr_getViewCount(void);
extern bool webxr_getViewPose(uint32_t view, float* position, float* orientation);
extern bool webxr_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down);
extern void webxr_getClipDistance(float* near, float* far);
extern void webxr_setClipDistance(float near, float far);
extern void webxr_getBoundsDimensions(float* width, float* depth);
extern const float* webxr_getBoundsGeometry(uint32_t* count);
extern bool webxr_getPose(Device device, float* position, float* orientation);
extern bool webxr_getVelocity(Device device, float* velocity, float* angularVelocity);
extern bool webxr_isDown(Device device, DeviceButton button, bool* down, bool* changed);
extern bool webxr_isTouched(Device device, DeviceButton button, bool* touched);
extern bool webxr_getAxis(Device device, DeviceAxis axis, float* value);
extern bool webxr_vibrate(Device device, float strength, float duration, float frequency);
extern struct ModelData* webxr_newModelData(Device device);
extern void webxr_renderTo(void (*callback)(void*), void* userdata);
extern void webxr_update(float dt);

HeadsetInterface lovrHeadsetWebXRDriver = {
  .driverType = DRIVER_WEBVR,
  .init = webxr_init,
  .destroy = webxr_destroy,
  .getName = webxr_getName,
  .getOriginType = webxr_getOriginType,
  .getDisplayTime = webxr_getDisplayTime,
  .getDisplayDimensions = webxr_getDisplayDimensions,
  .getDisplayMask = webxr_getDisplayMask,
  .getViewCount = webxr_getViewCount,
  .getViewPose = webxr_getViewPose,
  .getViewAngles = webxr_getViewAngles,
  .getClipDistance = webxr_getClipDistance,
  .setClipDistance = webxr_setClipDistance,
  .getBoundsDimensions = webxr_getBoundsDimensions,
  .getBoundsGeometry = webxr_getBoundsGeometry,
  .getPose = webxr_getPose,
  .getVelocity = webxr_getVelocity,
  .isDown = webxr_isDown,
  .isTouched = webxr_isTouched,
  .getAxis = webxr_getAxis,
  .vibrate = webxr_vibrate,
  .newModelData = webxr_newModelData,
  .renderTo = webxr_renderTo,
  .update = webxr_update
};
