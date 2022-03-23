#include "headset/headset.h"
#include "util.h"

HeadsetInterface* lovrHeadsetInterface = NULL;
static bool initialized = false;

bool lovrHeadsetInit(HeadsetDriver* drivers, size_t count, float supersample, float offset, uint32_t msaa, bool overlay) {
  if (initialized) return false;
  initialized = true;

  for (size_t i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
#ifdef LOVR_USE_DESKTOP
      case DRIVER_DESKTOP: interface = &lovrHeadsetDesktopDriver; break;
#endif
#ifdef LOVR_USE_OPENXR
      case DRIVER_OPENXR: interface = &lovrHeadsetOpenXRDriver; break;
#endif
#ifdef LOVR_USE_WEBXR
      case DRIVER_WEBXR: interface = &lovrHeadsetWebXRDriver; break;
#endif
      default: continue;
    }

    if (interface->init(supersample, offset, msaa, overlay)) {
      lovrHeadsetInterface = interface;
      break;
    }
  }

  lovrAssert(lovrHeadsetInterface, "No headset display driver available, check t.headset.drivers in conf.lua");
  return true;
}

void lovrHeadsetDestroy() {
  if (!initialized) return;
  initialized = false;
  if (lovrHeadsetInterface) {
    lovrHeadsetInterface->destroy();
    lovrHeadsetInterface = NULL;
  }
}
