#include "headset.h"
#include "vive.h"

static Headset* headset;

void lovrHeadsetInit() {
  headset = viveInit();
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  headset->interface->getPosition(headset, x, y, z);
}

void lovrHeadsetGetOrientation(float* x, float* y, float* z, float* w) {
  headset->interface->getOrientation(headset, x, y, z, w);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  headset->interface->getVelocity(headset, x, y, z);
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  headset->interface->getAngularVelocity(headset, x, y, z);
}

int lovrHeadsetIsPresent() {
  return headset->interface->isPresent(headset);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  headset->interface->renderTo(headset, callback, userdata);
}
