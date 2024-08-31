#include "headset/headset.h"
#include "util.h"
#include <stdatomic.h>

HeadsetInterface* lovrHeadsetInterface = NULL;
static uint32_t ref;

bool lovrHeadsetInit(HeadsetConfig* config) {
  if (atomic_fetch_add(&ref, 1)) return true;

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
  if (atomic_fetch_sub(&ref, 1) != 1) return;
  if (lovrHeadsetInterface) {
    lovrHeadsetInterface->destroy();
    lovrHeadsetInterface = NULL;
  }
  ref = 0;
}

void lovrLayerDestroy(void* ref) {
  lovrHeadsetInterface->destroyLayer(ref);
}
