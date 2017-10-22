#include "headset/headset.h"
#include "event/event.h"

void lovrControllerDestroy(const Ref* ref) {
  Controller* controller = containerof(ref, Controller);
  free(controller);
}


static HeadsetImpl* headset = NULL;


void lovrHeadsetInit() {
  // assert(headset==NULL)
  // TODO: should expose driver selection to lua, so conf can express a preference?

#if EMSCRIPTEN
  HeadsetImpl* drivers[] = { &lovrHeadsetWebVRDriver, &lovrHeadsetFakeDriver, NULL };
#else
  HeadsetImpl* drivers[] = { &lovrHeadsetOpenVRDriver, &lovrHeadsetFakeDriver, NULL };
#endif
  int i;
  for (i=0; drivers[i]; ++i ) {
    if (drivers[i]->isAvailable()) {
      headset = drivers[i];
      break;
    }
  }

  if( headset) {
    headset->init();
    atexit(lovrHeadsetDestroy);
  }
}


void lovrHeadsetDestroy() {
  if (headset) {
    headset->destroy();
    headset = NULL;    
  }
}

void lovrHeadsetPoll() {
  headset->poll();
}

int lovrHeadsetIsPresent() {
  return headset ? headset->isPresent() : 0;
}

HeadsetType lovrHeadsetGetType() {
  return headset ? headset->getType() : HEADSET_UNKNOWN;
}

HeadsetOrigin lovrHeadsetGetOriginType() {
  return headset ? headset->getOriginType() : ORIGIN_HEAD;
}

int lovrHeadsetIsMirrored() {
  return headset ? headset->isMirrored() : 0;
}

void lovrHeadsetSetMirrored(int mirror) {
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

void lovrHeadsetGetClipDistance(float* near, float* far) {
  if (!headset) {
    *near = *far = 0.f;
    return;
  }
  headset->getClipDistance(near, far);
}

void lovrHeadsetSetClipDistance(float near, float far) {
  if (headset) {
    headset->setClipDistance(near, far);
  }
}

float lovrHeadsetGetBoundsWidth() {
  return headset ? headset->getBoundsWidth() : 0.0f;
}

float lovrHeadsetGetBoundsDepth() {
  return headset ? headset->getBoundsDepth() : 0.0f;
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  if (!headset) {
    *geometry = 0.f;
    return;
  }

  headset->getBoundsGeometry(geometry);
}


void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getPosition(x, y, z);
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getEyePosition(eye,x, y, z);
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
  if (!headset) {
    *angle = *x = *y = *z = 0.f;
    return;
  }

  headset->getOrientation(angle,x,y,z);
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

int lovrHeadsetControllerIsPresent(Controller* controller) {
  if (!headset || !controller) {
    return 0;
  }

  return headset->controllerIsPresent(controller);
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return headset ? headset->controllerGetHand(controller) : HAND_UNKNOWN;
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  if (!headset || !controller) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->controllerGetPosition(controller, x, y, z);
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  if (!headset || !controller) {
    *angle = *x = *y = *z = 0.f;
    return;
  }

  headset->controllerGetOrientation(controller, angle, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  if (!headset || !controller) {
    return 0.f;
  }

  return headset->controllerGetAxis(controller, axis);
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  if (!headset || !controller) {
    return 0;
  }

  return headset->controllerIsDown(controller, button);
}

int lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button) {
  return (headset && controller) ? headset->controllerIsTouched(controller,button) : 0;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
  if (!headset || !controller) {
    return;
  }

  headset->controllerVibrate(controller, duration, power);
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller)
{
  if( headset && controller) {
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
  if(headset) {
    headset->update(dt);
  }
}
