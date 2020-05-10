#include "spatializer.h"
#include <phonon.h>

static bool steamaudio_init() {
  return false;
}

static void steamaudio_apply(const float* input, float* output, uint32_t frames, float* transform) {
  //
}

Spatializer lovrSteamAudioSpatializer = {
  .init = steamaudio_init,
  .apply = steamaudio_apply
};
