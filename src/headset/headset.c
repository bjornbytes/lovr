#include "headset/headset.h"
#include "event/event.h"

static HeadsetInterface* headset = NULL;
static bool initialized = false;

void lovrHeadsetInit(HeadsetDriver* drivers, int count) {
  if (initialized) return;
  initialized = true;
  headset = NULL;

  for (int i = 0; i < count; i++) {
    HeadsetInterface* interface = NULL;

    switch (drivers[i]) {
      case DRIVER_FAKE: interface = &lovrHeadsetFakeDriver; break;
#ifndef EMSCRIPTEN
      case DRIVER_OPENVR: interface = &lovrHeadsetOpenVRDriver; break;
#else
      case DRIVER_WEBVR: interface = &lovrHeadsetWebVRDriver; break;
#endif
      default: break;
    }

    if (interface && interface->init()) {
      headset = interface;
      break;
    }
  }
}

void lovrHeadsetDestroy() {
  if (!initialized) return;
  initialized = false;

  if (headset) {
    headset->destroy();
    headset = NULL;
  }
}

const HeadsetDriver* lovrHeadsetGetDriver() {
  if (!headset) {
    lovrThrow("Headset is not initialized");
  }

  return &headset->driverType;
}

HeadsetType lovrHeadsetGetType() {
  return headset ? headset->getType() : HEADSET_UNKNOWN;
}

HeadsetOrigin lovrHeadsetGetOriginType() {
  return headset ? headset->getOriginType() : ORIGIN_HEAD;
}

bool lovrHeadsetIsMounted() {
  return headset ? headset->isMounted() : false;
}

bool lovrHeadsetIsMirrored() {
  return headset ? headset->isMirrored() : false;
}

void lovrHeadsetSetMirrored(bool mirror) {
  if (headset) {
    headset->setMirrored(mirror);
  }
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  if (!headset) {
    *width = *height = 0;
    return;
  }

  headset->getDisplayDimensions(width, height);
}

void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar) {
  if (!headset) {
    *clipNear = *clipFar = 0.f;
    return;
  }
  headset->getClipDistance(clipNear, clipFar);
}

void lovrHeadsetSetClipDistance(float clipNear, float clipFar) {
  if (headset) {
    headset->setClipDistance(clipNear, clipFar);
  }
}

float lovrHeadsetGetBoundsWidth() {
  return headset ? headset->getBoundsWidth() : 0.f;
}

float lovrHeadsetGetBoundsDepth() {
  return headset ? headset->getBoundsDepth() : 0.f;
}

void lovrHeadsetGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  if (!headset) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  headset->getPose(x, y, z, angle, ax, ay, az);
}

void lovrHeadsetGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  if (!headset) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  headset->getEyePose(eye, x, y, z, angle, ax, ay, az);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getVelocity(x,y,z);
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getAngularVelocity(x,y,z);
}

vec_controller_t* lovrHeadsetGetControllers() {
  if (!headset) {
    return NULL;
  }

  return headset->getControllers();
}

bool lovrHeadsetControllerIsConnected(Controller* controller) {
  if (!headset || !controller) {
    return false;
  }

  return headset->controllerIsConnected(controller);
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return headset ? headset->controllerGetHand(controller) : HAND_UNKNOWN;
}

void lovrHeadsetControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  if (!headset || !controller) {
    *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
    return;
  }

  headset->controllerGetPose(controller, x, y, z, angle, ax, ay, az);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  if (!headset || !controller) {
    return 0.f;
  }

  return headset->controllerGetAxis(controller, axis);
}

bool lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  if (!headset || !controller) {
    return false;
  }

  return headset->controllerIsDown(controller, button);
}

bool lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button) {
  return (headset && controller) ? headset->controllerIsTouched(controller,button) : false;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
  if (!headset || !controller) {
    return;
  }

  headset->controllerVibrate(controller, duration, power);
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  if (headset && controller) {
    return headset->controllerNewModelData(controller);
  } else {
    return NULL;
  }
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  if (headset) {
    headset->renderTo(callback, userdata);
  }
}

void lovrHeadsetUpdate(float dt) {
  if (headset && headset->update) {
    headset->update(dt);
  }
}
