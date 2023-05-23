#include "headset/headset.h"
#include "util.h"
#include "core/maf.h"
#include <string.h>

HeadsetInterface* lovrHeadsetInterface = NULL;

static struct {
  bool initialized;
  float position[4];
  float orientation[4];
} state;

bool lovrHeadsetInit(HeadsetConfig* config) {
  if (state.initialized) return false;
  state.initialized = true;

  for (size_t i = 0; i < config->driverCount; i++) {
    HeadsetInterface* interface = NULL;

    switch (config->drivers[i]) {
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

    if (interface->init(config)) {
      lovrHeadsetInterface = interface;
      break;
    }
  }

  lovrAssert(lovrHeadsetInterface, "No headset display driver available, check t.headset.drivers in conf.lua");
  state.orientation[3] = 1.f;
  return true;
}

void lovrHeadsetDestroy(void) {
  if (!state.initialized) return;
  if (lovrHeadsetInterface) {
    lovrHeadsetInterface->destroy();
    lovrHeadsetInterface = NULL;
  }
  memset(&state, 0, sizeof(state));
}

void lovrHeadsetGetOffset(float* position, float* orientation) {
  memcpy(position, state.position, 4 * sizeof(float));
  memcpy(orientation, state.orientation, 4 * sizeof(float));
}

void lovrHeadsetSetOffset(float* position, float* orientation) {
  memcpy(state.position, position, 4 * sizeof(float));
  memcpy(state.orientation, orientation, 4 * sizeof(float));
}

void lovrHeadsetApplyOffset(float* position, float* orientation) {
  quat_rotate(state.orientation, position);
  vec3_add(position, state.position);
  quat_mul(orientation, state.orientation, orientation);
}
