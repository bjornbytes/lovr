#include "headset/headset.h"
#include "util.h"

HeadsetInterface* lovrHeadsetInterface = NULL;
static bool initialized = false;

bool lovrHeadsetInit(HeadsetConfig* config) {
  if (initialized) return false;
  initialized = true;

  for (size_t i = 0; i < config->driverCount; i++) {
    HeadsetInterface* interface = NULL;

    switch (config->drivers[i]) {
#ifdef LOVR_USE_SIMULATOR
      case DRIVER_SIMULATOR: interface = &lovrHeadsetSimulatorDriver; break;
#endif
#ifdef LOVR_USE_OPENXR
      case DRIVER_OPENXR: interface = &lovrHeadsetOpenXRDriver; break;
#endif
#ifdef LOVR_USE_WEBXR
      case DRIVER_WEBXR: interface = &lovrHeadsetWebXRDriver; break;
#endif
      default: continue;
    }

    if (interface->init(config)) {
      lovrHeadsetInterface = interface;
      break;
    }
  }

  lovrAssert(lovrHeadsetInterface, "No headset display driver available, check t.headset.drivers in conf.lua");
  return true;
}

void lovrHeadsetDestroy(void) {
  if (!initialized) return;
  initialized = false;
  if (lovrHeadsetInterface) {
    lovrHeadsetInterface->destroy();
    lovrHeadsetInterface = NULL;
  }
}

void lovrLayerDestroy(void* ref) {
  lovrHeadsetInterface->destroyLayer(ref);
}
