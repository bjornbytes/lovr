#include "spatializer.h"
#include <phonon.h>

bool phonon_init(SpatializerConfig config) {
  return false;
}

void phonon_destroy() {
  //
}

uint32_t phonon_apply(Source* source, const float* input, float* output, uint32_t frames, uint32_t _frames) {
  return 0;
}

uint32_t phonon_tail(float* scratch, float* output, uint32_t frames) {
  return 0;
}

void phonon_setListenerPose(float position[4], float orientation[4]) {
  //
}

void phonon_sourceCreate(Source* source) {
  //
}

void phonon_sourceDestroy(Source* source) {
  //
}

Spatializer phononSpatializer = {
  .init = phonon_init,
  .destroy = phonon_destroy,
  .apply = phonon_apply,
  .tail = phonon_tail,
  .setListenerPose = phonon_setListenerPose,
  .sourceCreate = phonon_sourceCreate,
  .sourceDestroy = phonon_sourceDestroy,
  .name = "phonon"
};
