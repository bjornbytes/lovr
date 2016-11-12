#include "headset.h"
#include "vive.h"

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

char lovrHeadsetIsControllerPresent(Controller* controller) {
  return headset->interface->isControllerPresent(headset, controller);
}

void lovrHeadsetGetControllerPosition(Controller* controller, float* x, float* y, float* z) {
  headset->interface->getControllerPosition(headset, controller, x, y, z);
}

void lovrHeadsetGetControllerOrientation(Controller* controller, float* w, float* x, float* y, float* z) {
  headset->interface->getControllerOrientation(headset, controller, w, x, y, z);
}

float lovrHeadsetGetControllerAxis(Controller* controller) {
  return headset->interface->getControllerAxis(headset, controller);
}

ControllerHand lovrHeadsetGetControllerHand(Controller* controller) {
  return headset->interface->getControllerHand(headset, controller);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  headset->interface->renderTo(headset, callback, userdata);
}
