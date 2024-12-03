#include "headset/headset.h"

extern bool webxr_init(HeadsetConfig* config);
extern bool webxr_start(void);
extern void webxr_stop(void);
extern void webxr_destroy(void);
extern bool webxr_getDriverName(char* name, size_t length);
extern void webxr_getFeatures(HeadsetFeatures* features);
extern bool webxr_getName(char* name, size_t length);
extern bool webxr_isSeated(void);
extern void webxr_getDisplayDimensions(uint32_t* width, uint32_t* height);
extern float webxr_getRefreshRate(void);
extern bool webxr_setRefreshRate(float refreshRate);
extern const float* webxr_getRefreshRates(uint32_t* count);
PassthroughMode webxr_getPassthrough(void);
extern bool webxr_setPassthrough(PassthroughMode mode);
extern bool webxr_isPassthroughSupported(PassthroughMode mode);
extern double webxr_getDisplayTime(void);
extern double webxr_getDeltaTime(void);
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
extern bool webxr_getSkeleton(Device device, float* poses, SkeletonSource* source);
extern bool webxr_vibrate(Device device, float strength, float duration, float frequency);
extern void webxr_stopVibration(Device device);
extern struct ModelData* webxr_newModelData(Device device, bool animated);
extern bool webxr_animate(struct Model* model);
extern bool webxr_getTexture(Texture** texture);
extern bool webxr_getPass(Pass** pass);
extern bool webxr_submit(void);
extern bool webxr_isActive(void);
extern bool webxr_isVisible(void);
extern bool webxr_isFocused(void);
extern bool webxr_isMounted(void);
extern bool webxr_update(double* dt);

static bool webxrAttached = false;
static HeadsetInterface* previousHeadsetDriver;

void webxr_attach(void) {
  if (webxrAttached || lovrHeadsetInterface == &lovrHeadsetWebXRDriver) {
    return;
  }

  previousHeadsetDriver = lovrHeadsetInterface;
  lovrHeadsetInterface = &lovrHeadsetWebXRDriver;
  webxrAttached = true;
}

void webxr_detach(void) {
  if (!webxrAttached) {
    return;
  }

  lovrHeadsetInterface = previousHeadsetDriver;
  previousHeadsetDriver = NULL;
  webxrAttached = false;
}

HeadsetInterface lovrHeadsetWebXRDriver = {
  .driverType = DRIVER_WEBXR,
  .init = webxr_init,
  .start = webxr_start,
  .stop = webxr_stop,
  .destroy = webxr_destroy,
  .getFeatures = webxr_getFeatures,
  .getDriverName = webxr_getDriverName,
  .getName = webxr_getName,
  .isSeated = webxr_isSeated,
  .getDisplayDimensions = webxr_getDisplayDimensions,
  .getRefreshRate = webxr_getRefreshRate,
  .setRefreshRate = webxr_setRefreshRate,
  .getRefreshRates = webxr_getRefreshRates,
  .getPassthrough = webxr_getPassthrough,
  .setPassthrough = webxr_setPassthrough,
  .isPassthroughSupported = webxr_isPassthroughSupported,
  .getDisplayTime = webxr_getDisplayTime,
  .getDeltaTime = webxr_getDeltaTime,
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
  .stopVibration = webxr_stopVibration,
  .newModelData = webxr_newModelData,
  .animate = webxr_animate,
  .getTexture = webxr_getTexture,
  .getPass = webxr_getPass,
  .submit = webxr_submit,
  .isVisible = webxr_isVisible,
  .isFocused = webxr_isFocused,
  .isMounted = webxr_isMounted,
  .update = webxr_update
};
