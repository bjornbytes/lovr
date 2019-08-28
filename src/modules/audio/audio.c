#include "audio/audio.h"
#include "audio/source.h"
#include "modules/data/soundData.h"
#include "core/arr.h"
#include "core/maf.h"
#include "core/ref.h"
#include "util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>

static struct {
  bool initialized;
  Source* sources;
  ma_device device;
  ma_mutex lock;
} state;

static void handler(ma_device* device, void* output, const void* input, uint32_t frames) {
  ma_mutex_lock(&state.lock);

  //float buffer[256];

  for (Source** s = &state.sources; *s;) {
    Source* source = *s;

    if (!source->playing) {
      remove:
      *s = source->next;
      source->tracked = false;
      lovrRelease(Source, source);
      continue;
    }

    uint32_t n = 0;
    decode:
    n += lovrDecoderDecode(source->decoder, frames - n, device->playback.channels, (uint8_t*) output + n * FRAME_SIZE(device->playback.channels));

    if (n < frames) {
      if (source->looping) {
        lovrDecoderSeek(source->decoder, 0);
        goto decode;
      } else {
        source->playing = false;
        goto remove;
      }
    }

    s = &source->next;
  }

  ma_mutex_unlock(&state.lock);
}

bool lovrAudioInit() {
  if (state.initialized) return false;

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = SAMPLE_RATE;
  config.dataCallback = handler;

  if (ma_device_init(NULL, &config, &state.device)) {
    return false;
  }

  if (ma_mutex_init(state.device.pContext, &state.lock)) {
    ma_device_uninit(&state.device);
    return false;
  }

  if (ma_device_start(&state.device)) {
    ma_device_uninit(&state.device);
    return false;
  }

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.device);
  ma_mutex_uninit(&state.lock);
  while (state.sources) {
    Source* source = state.sources;
    state.sources = source->next;
    source->next = NULL;
    source->tracked = false;
    lovrRelease(Source, source);
  }
  memset(&state, 0, sizeof(state));
}

Source* lovrSourceInit(Source* source, Decoder* decoder) {
  source->volume = 1.f;
  source->decoder = decoder;
  lovrRetain(decoder);
  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrAssert(!source->tracked, "Unreachable");
  lovrRelease(Decoder, source->decoder);
}

void lovrSourcePlay(Source* source) {
  ma_mutex_lock(&state.lock);

  source->playing = true;

  if (!source->tracked) {
    lovrRetain(source);
    source->tracked = true;
    source->next = state.sources;
    state.sources = source;
  }

  ma_mutex_unlock(&state.lock);
}

void lovrSourcePause(Source* source) {
  source->playing = false;
}

bool lovrSourceIsPlaying(Source* source) {
  return source->playing;
}

bool lovrSourceIsLooping(Source* source) {
  return source->looping;
}

void lovrSourceSetLooping(Source* source, bool loop) {
  source->looping = loop;
}

float lovrSourceGetVolume(Source* source) {
  return source->volume;
}

void lovrSourceSetVolume(Source* source, float volume) {
  ma_mutex_lock(&state.lock);
  source->volume = volume;
  ma_mutex_unlock(&state.lock);
}

Decoder* lovrSourceGetDecoder(Source* source) {
  return source->decoder;
}