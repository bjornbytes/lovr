#include "headset/headset.h"
#include "event/event.h"

HeadsetInterface* lovrHeadsetDriver = NULL;
static bool initialized = false;

void lovrHeadsetInit(HeadsetDriver* drivers, int count, float offset, int msaa) {
  if (initialized) return;
  initialized = true;

  for (int i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
      case DRIVER_FAKE: interface = &lovrHeadsetFakeDriver; break;
#ifndef EMSCRIPTEN
#ifdef USE_OCULUS
      case DRIVER_OCULUS: interface = &lovrHeadsetOculusDriver; break;
#endif
      case DRIVER_OPENVR: interface = &lovrHeadsetOpenVRDriver; break;
#else
      case DRIVER_WEBVR: interface = &lovrHeadsetWebVRDriver; break;
#endif
      default: break;
    }

    if (interface && interface->init(offset, msaa)) {
      lovrHeadsetDriver = interface;
      break;
    }
  }

  lovrAssert(lovrHeadsetDriver, "No headset driver available, check t.headset.drivers in conf.lua");
}

void lovrHeadsetDestroy() {
  if (!initialized) return;
  initialized = false;

  if (lovrHeadsetDriver) {
    lovrHeadsetDriver->destroy();
    lovrHeadsetDriver = NULL;
  }
}
