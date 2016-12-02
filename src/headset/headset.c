#include "headset/headset.h"
#include "headset/vive.h"
#include "event/event.h"

static Headset* headset;

void lovrHeadsetInit() {
  headset = viveInit();

  if (headset) {
    lovrEventAddPump(lovrHeadsetPoll);
  }
}

void lovrHeadsetPoll() {
  headset->poll(headset);
}

char lovrHeadsetIsPresent() {
  if (!headset) {
    return 0;
  }

  return headset->isPresent(headset);
}

const char* lovrHeadsetGetType() {
  if (!headset) {
    return NULL;
  }

  return headset->getType(headset);
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  if (!headset) {
    *width = *height = 0;
    return;
  }

  headset->getDisplayDimensions(headset, width, height);
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  if (!headset) {
    *near = *far = 0.f;
    return;
  }

  headset->getClipDistance(headset, near, far);
}

void lovrHeadsetSetClipDistance(float near, float far) {
  if (!headset) {
    return;
  }

  headset->setClipDistance(headset, near, far);
}

float lovrHeadsetGetBoundsWidth() {
  if (!headset) {
    return 0.f;
  }

  return headset->getBoundsWidth(headset);
}

float lovrHeadsetGetBoundsDepth() {
  if (!headset) {
    return 0.f;
  }

  return headset->getBoundsDepth(headset);
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  if (!headset) {
    *geometry = 0.f;
    return;
  }

  headset->getBoundsGeometry(headset, geometry);
}

char lovrHeadsetIsBoundsVisible() {
  if (!headset) {
    return 0;
  }

  return headset->isBoundsVisible(headset);
}

void lovrHeadsetSetBoundsVisible(char visible) {
  if (!headset) {
    return;
  }

  headset->setBoundsVisible(headset, visible);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getPosition(headset, x, y, z);
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {
  if (!headset) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  headset->getOrientation(headset, w, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getVelocity(headset, x, y, z);
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  if (!headset) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->getAngularVelocity(headset, x, y, z);
}

vec_controller_t* lovrHeadsetGetControllers() {
  if (!headset) {
    return NULL;
  }

  return headset->getControllers(headset);
}

char lovrHeadsetControllerIsPresent(Controller* controller) {
  if (!headset || !controller) {
    return 0;
  }

  return headset->controllerIsPresent(headset, controller);
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  if (!headset || !controller) {
    *x = *y = *z = 0.f;
    return;
  }

  headset->controllerGetPosition(headset, controller, x, y, z);
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z) {
  if (!headset || !controller) {
    *w = *x = *y = *z = 0.f;
    return;
  }

  headset->controllerGetOrientation(headset, controller, w, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  if (!headset || !controller) {
    return 0.f;
  }

  return headset->controllerGetAxis(headset, controller, axis);
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  if (!headset || !controller) {
    return 0;
  }

  return headset->controllerIsDown(headset, controller, button);
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration) {
  if (!headset || !controller) {
    return;
  }

  headset->controllerVibrate(headset, controller, duration);
}

void* lovrHeadsetControllerGetModel(Controller* controller, ControllerModelFormat* format) {
  if (!headset || !controller) {
    return NULL;
  }

  return headset->controllerGetModel(headset, controller, format);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  if (!headset) {
    return;
  }

  headset->renderTo(headset, callback, userdata);
}

void lovrControllerDestroy(const Ref* ref) {
  Controller* controller = containerof(ref, Controller);
  free(controller);
}
