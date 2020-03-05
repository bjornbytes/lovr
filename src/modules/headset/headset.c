#include "headset/headset.h"
#include "core/util.h"

HeadsetInterface* lovrHeadsetDriver = NULL;
HeadsetInterface* lovrHeadsetTrackingDrivers = NULL;
static bool initialized = false;

bool lovrHeadsetInit(HeadsetDriver* drivers, size_t count, float offset, uint32_t msaa) {
  if (initialized) return false;
  initialized = true;

  HeadsetInterface* lastTrackingDriver = NULL;

  for (size_t i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
#ifdef LOVR_USE_DESKTOP_HEADSET
      case DRIVER_DESKTOP: interface = &lovrHeadsetDesktopDriver; break;
#endif
#ifdef LOVR_USE_LEAP
      case DRIVER_LEAP_MOTION: interface = &lovrHeadsetLeapMotionDriver; break;
#endif
#ifdef LOVR_USE_OCULUS
      case DRIVER_OCULUS: interface = &lovrHeadsetOculusDriver; break;
#endif
#ifdef LOVR_USE_OCULUS_MOBILE
      case DRIVER_OCULUS_MOBILE: interface = &lovrHeadsetOculusMobileDriver; break;
#endif
#ifdef LOVR_USE_OPENVR
      case DRIVER_OPENVR: interface = &lovrHeadsetOpenVRDriver; break;
#endif
#ifdef LOVR_USE_OPENXR
      case DRIVER_OPENXR: interface = &lovrHeadsetOpenXRDriver; break;
#endif
#ifdef LOVR_USE_WEBVR
      case DRIVER_WEBVR: interface = &lovrHeadsetWebVRDriver; break;
#endif
#ifdef LOVR_USE_WEBXR
      case DRIVER_WEBXR: interface = &lovrHeadsetWebXRDriver; break;
#endif
      default: continue;
    }

    bool hasDisplay = interface->renderTo != NULL;
    bool shouldInitialize = !hasDisplay || !lovrHeadsetDriver;

    if (shouldInitialize && interface->init(offset, msaa)) {
      if (hasDisplay) {
        lovrHeadsetDriver = interface;
      }

      if (lastTrackingDriver) {
        lastTrackingDriver->next = interface;
      } else {
        lovrHeadsetTrackingDrivers = interface;
      }

      lastTrackingDriver = interface;
    }
  }

  lovrAssert(lovrHeadsetDriver, "No headset display driver available, check t.headset.drivers in conf.lua");
  return true;
}

void lovrHeadsetDestroy() {
  if (!initialized) return;
  initialized = false;

  HeadsetInterface* driver = lovrHeadsetTrackingDrivers;
  while (driver) {
    if (driver != lovrHeadsetDriver) {
      driver->destroy();
    }
    HeadsetInterface* next = driver->next;
    driver->next = NULL;
    driver = next;
  }

  if (lovrHeadsetDriver) {
    lovrHeadsetDriver->destroy();
    lovrHeadsetDriver = NULL;
  }
}
