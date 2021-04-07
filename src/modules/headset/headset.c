#include "headset/headset.h"
#include "core/util.h"

HeadsetInterface* lovrHeadsetDisplayDriver = NULL;
HeadsetInterface* lovrHeadsetTrackingDrivers = NULL;
static bool initialized = false;

bool lovrHeadsetInit(HeadsetDriver* drivers, size_t count, float supersample, float offset, uint32_t msaa, bool overlay) {
  if (initialized) return false;
  initialized = true;

  HeadsetInterface** trackingDrivers = &lovrHeadsetTrackingDrivers;

  for (size_t i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
#ifdef LOVR_USE_DESKTOP
      case DRIVER_DESKTOP: interface = &lovrHeadsetDesktopDriver; break;
#endif
#ifdef LOVR_USE_OCULUS
      case DRIVER_OCULUS: interface = &lovrHeadsetOculusDriver; break;
#endif
#ifdef LOVR_USE_OPENVR
      case DRIVER_OPENVR: interface = &lovrHeadsetOpenVRDriver; break;
#endif
#ifdef LOVR_USE_OPENXR
      case DRIVER_OPENXR: interface = &lovrHeadsetOpenXRDriver; break;
#endif
#ifdef LOVR_USE_VRAPI
      case DRIVER_VRAPI: interface = &lovrHeadsetVrApiDriver; break;
#endif
#ifdef LOVR_USE_PICO
      case DRIVER_PICO: interface = &lovrHeadsetPicoDriver; break;
#endif
#ifdef LOVR_USE_WEBXR
      case DRIVER_WEBXR: interface = &lovrHeadsetWebXRDriver; break;
#endif
      default: continue;
    }

    bool hasDisplay = interface->renderTo != NULL;
    bool shouldInitialize = !hasDisplay || !lovrHeadsetDisplayDriver;

    if (shouldInitialize && interface->init(supersample, offset, msaa, overlay)) {
      if (hasDisplay) {
        lovrHeadsetDisplayDriver = interface;
      }

      *trackingDrivers = interface;
      trackingDrivers = &interface->next;
    }
  }

  lovrAssert(lovrHeadsetDisplayDriver, "No headset display driver available, check t.headset.drivers in conf.lua");
  return true;
}

void lovrHeadsetDestroy() {
  if (!initialized) return;
  initialized = false;

  HeadsetInterface* driver = lovrHeadsetTrackingDrivers;
  while (driver) {
    if (driver != lovrHeadsetDisplayDriver) {
      driver->destroy();
    }
    HeadsetInterface* next = driver->next;
    driver->next = NULL;
    driver = next;
  }

  if (lovrHeadsetDisplayDriver) {
    lovrHeadsetDisplayDriver->destroy();
    lovrHeadsetDisplayDriver = NULL;
  }
}
