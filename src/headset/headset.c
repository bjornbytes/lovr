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

  return headset->isPresent(headset);
}

const char* lovrHeadsetGetType() {
  return headset->getType(headset);
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  headset->getDisplayDimensions(headset, width, height);
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  headset->getClipDistance(headset, near, far);
}

void lovrHeadsetSetClipDistance(float near, float far) {
  headset->setClipDistance(headset, near, far);
}

float lovrHeadsetGetBoundsWidth() {
  return headset->getBoundsWidth(headset);
}

float lovrHeadsetGetBoundsDepth() {
  return headset->getBoundsDepth(headset);
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  headset->getBoundsGeometry(headset, geometry);
}

char lovrHeadsetIsBoundsVisible() {
  return headset->isBoundsVisible(headset);
}

void lovrHeadsetSetBoundsVisible(char visible) {
  headset->setBoundsVisible(headset, visible);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  headset->getPosition(headset, x, y, z);
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {
  headset->getOrientation(headset, w, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  headset->getVelocity(headset, x, y, z);
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  headset->getAngularVelocity(headset, x, y, z);
}

Controller* lovrHeadsetGetController(ControllerHand hand) {
  return headset->getController(headset, hand);
}

char lovrHeadsetControllerIsPresent(Controller* controller) {
  return headset->controllerIsPresent(headset, controller);
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  headset->controllerGetPosition(headset, controller, x, y, z);
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* w, float* x, float* y, float* z) {
  headset->controllerGetOrientation(headset, controller, w, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return headset->controllerGetAxis(headset, controller, axis);
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  return headset->controllerIsDown(headset, controller, button);
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return headset->controllerGetHand(headset, controller);
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration) {
  headset->controllerVibrate(headset, controller, duration);
}

void* lovrHeadsetControllerGetModel(Controller* controller, ControllerModelFormat* format) {
  return headset->controllerGetModel(headset, controller, format);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  headset->renderTo(headset, callback, userdata);
}
