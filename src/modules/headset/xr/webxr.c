#include "headset/headset.h"

extern bool webxr_init(float supersample, float offset, uint32_t msaa);
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
extern bool webxr_getSkeleton(Device device, float* poses);
extern bool webxr_vibrate(Device device, float strength, float duration, float frequency);
extern struct ModelData* webxr_newModelData(Device device, bool animated);
extern bool webxr_animate(Device device, struct Model* model);
extern void webxr_renderTo(void (*callback)(void*), void* userdata);
extern void webxr_update(float dt);

static bool webxrAttached = false;
static HeadsetInterface* previousHeadsetDriver;

static void setDriver(HeadsetInterface* new) {
  if (lovrHeadsetTrackingDrivers == lovrHeadsetDisplayDriver) {
    lovrHeadsetTrackingDrivers = new;
  } else {
    FOREACH_TRACKING_DRIVER(driver) {
      if (driver->next == lovrHeadsetDisplayDriver) {
        driver->next = new;
        break;
      }
    }
  }

  new->next = lovrHeadsetDisplayDriver->next;
  lovrHeadsetDisplayDriver->next = NULL;
  lovrHeadsetDisplayDriver = new;
}

void webxr_attach() {
  if (webxrAttached || lovrHeadsetDisplayDriver == &lovrHeadsetWebXRDriver) {
    return;
  }

  previousHeadsetDriver = lovrHeadsetDisplayDriver;
  setDriver(&lovrHeadsetWebXRDriver);
  webxrAttached = true;
}

void webxr_detach() {
  if (!webxrAttached) {
    return;
  }

  setDriver(previousHeadsetDriver);
  previousHeadsetDriver = NULL;
  webxrAttached = false;
}

HeadsetInterface lovrHeadsetWebXRDriver = {
  .driverType = DRIVER_WEBXR,
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
  .getSkeleton = webxr_getSkeleton,
  .vibrate = webxr_vibrate,
  .newModelData = webxr_newModelData,
  .animate = webxr_animate,
  .renderTo = webxr_renderTo,
  .update = webxr_update
};
