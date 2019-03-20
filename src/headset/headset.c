#include "headset/headset.h"
#include "util.h"

HeadsetInterface* lovrHeadsetDriver = NULL;
HeadsetInterface* lovrHeadsetTrackingDrivers = NULL;
static bool initialized = false;

bool lovrHeadsetInit(HeadsetDriver* drivers, int count, float offset, int msaa) {
  if (initialized) return false;
  initialized = true;

  HeadsetInterface* lastTrackingDriver = NULL;

  for (int i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
#ifdef LOVR_USE_DESKTOP_HEADSET
      case DRIVER_DESKTOP: interface = &lovrHeadsetDesktopDriver; break;
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
#ifdef LOVR_USE_WEBVR
      case DRIVER_WEBVR: interface = &lovrHeadsetWebVRDriver; break;
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

void lovrControllerDestroy(void* ref) {
  //
}
