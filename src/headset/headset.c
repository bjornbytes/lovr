#include "headset/headset.h"
#include "headset/vive.h"

static Headset* headset;

void lovrHeadsetInit() {
  headset = viveInit();
}

char lovrHeadsetIsPresent() {
  if (!headset) {
    return 0;
  }

  return headset->interface->isPresent(headset);
}

const char* lovrHeadsetGetType() {
  return headset->interface->getType(headset);
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  headset->interface->getDisplayDimensions(headset, width, height);
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  headset->interface->getClipDistance(headset, near, far);
}

void lovrHeadsetSetClipDistance(float near, float far) {
  headset->interface->setClipDistance(headset, near, far);
}

void lovrHeadsetGetTrackingSize(float* width, float* depth) {
  headset->interface->getTrackingSize(headset, width, depth);
}

char lovrHeadsetIsBoundsVisible() {
  return headset->interface->isBoundsVisible(headset);
}

void lovrHeadsetSetBoundsVisible(char visible) {
  headset->interface->setBoundsVisible(headset, visible);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  headset->interface->getPosition(headset, x, y, z);
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {
  headset->interface->getOrientation(headset, w, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  headset->interface->getVelocity(headset, x, y, z);
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  headset->interface->getAngularVelocity(headset, x, y, z);
}

Controller* lovrHeadsetGetController(ControllerHand hand) {
  return headset->interface->getController(headset, hand);
}

char lovrHeadsetControllerIsPresent(Controller* controller) {
  return headset->interface->controllerIsPresent(headset, controller);
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  headset->interface->controllerGetPosition(headset, controller, x, y, z);
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z) {
  headset->interface->controllerGetOrientation(headset, controller, w, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return headset->interface->controllerGetAxis(headset, controller, axis);
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  return headset->interface->controllerIsDown(headset, controller, button);
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return headset->interface->controllerGetHand(headset, controller);
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration) {
  headset->interface->controllerVibrate(headset, controller, duration);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  headset->interface->renderTo(headset, callback, userdata);
}
