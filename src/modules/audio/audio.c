#include "audio/audio.h"
#include "data/soundData.h"
#include "core/arr.h"
#include "core/ref.h"
#include "core/ref.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>
#include "lib/miniaudio/miniaudio.h"

#define SAMPLE_RATE 44100

static const ma_format formats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

struct Source {
  Source* next;
  SoundData* sound;
  ma_data_converter* converter;
  uint32_t offset;
  float volume;
  bool tracked;
  bool playing;
  bool looping;
  bool spatial;
};

typedef struct {
  bool (*init)(void);
  void (*destroy)(void);
  void (*apply)(Source* source, const void* input, float* output, uint32_t frames);
} Spatializer;

static struct {
  bool initialized;
  ma_context context;
  AudioConfig config[2];
  ma_device devices[2];
  ma_mutex locks[2];
  Source* sources;
  arr_t(ma_data_converter) converters;
  Spatializer* spatializer;
} state;

// Device callbacks

static void onPlayback(ma_device* device, void* output, const void* _, uint32_t frames) {
  ma_mutex_lock(&state.locks[0]);

  float rawBuffer[1024];
  float mixBuffer[1024];

  size_t stride = sizeof(rawBuffer) / 2 / sizeof(float);

  for (Source** list = &state.sources, *source = *list; source != NULL; source = *list) {
    if (!source->playing) {
      *list = source->next;
      source->tracked = false;
      lovrRelease(Source, source);
      continue;
    }

    for (uint32_t f = 0; f < frames; f += stride) {
      uint32_t count = MIN(stride, frames - f);
      uint32_t n = source->sound->read(source->sound, source->offset, count, rawBuffer);

      memcpy(mixBuffer, rawBuffer, count * stride);

      float* p = (float*) output + f * 2;
      for (uint32_t i = 0; i < n * 2; i++) {
        p[i] += mixBuffer[i] * source->volume;
      }

      if (n < count) {
        source->offset = 0;
        if (!source->looping) {
          source->playing = false;
          continue;
        }
      } else {
        source->offset += n;
      }
    }

    list = &source->next;
  }

  ma_mutex_unlock(&state.locks[0]);
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t frames) {
  ma_mutex_lock(&state.locks[1]);
  ma_mutex_unlock(&state.locks[1]);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

// Spatializers

static bool phonon_init(void) {
  return false;
}

static void phonon_destroy(void) {
  //
}

static void phonon_apply(Source* source, const void* input, float* output, uint32_t frames) {
  //
}

static Spatializer spatializers[] = {
  { phonon_init, phonon_destroy, phonon_apply }
};

// Entry

bool lovrAudioInit(AudioConfig config[2]) {
  if (state.initialized) return false;

  memcpy(state.config, config, sizeof(state.config));

  if (ma_context_init(NULL, 0, NULL, &state.context)) {
    return false;
  }

  lovrAudioReset();

  for (size_t i = 0; i < 2; i++) {
    if (config[i].enable) {
      if (ma_mutex_init(&state.context, &state.locks[i])) {
        lovrAudioDestroy();
        return false;
      }

      if (config[i].start) {
        if (ma_device_start(&state.devices[i])) {
          lovrAudioDestroy();
          return false;
        }
      }
    }
  }

  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (spatializers[i].init()) {
      state.spatializer = &spatializers[i];
      break;
    }
  }

  arr_init(&state.converters);

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.devices[0]);
  ma_device_uninit(&state.devices[1]);
  ma_mutex_uninit(&state.locks[0]);
  ma_mutex_uninit(&state.locks[1]);
  ma_context_uninit(&state.context);
  if (state.spatializer) state.spatializer->destroy();
  arr_free(&state.converters);
  memset(&state, 0, sizeof(state));
}

bool lovrAudioReset() {
  for (size_t i = 0; i < 2; i++) {
    if (state.config[i].enable) {
      ma_device_type deviceType = (i == 0) ? ma_device_type_playback : ma_device_type_capture;

      ma_device_config config = ma_device_config_init(deviceType);
      config.sampleRate = SAMPLE_RATE;
      config.playback.format = ma_format_f32;
      config.capture.format = ma_format_f32;
      config.playback.channels = 2;
      config.capture.channels = 1;
      config.dataCallback = callbacks[i];

      if (ma_device_init(&state.context, &config, &state.devices[i])) {
        return false;
      }
    }
  }

  return true;
}

bool lovrAudioStart(AudioType type) {
  return !ma_device_start(&state.devices[type]);
}

bool lovrAudioStop(AudioType type) {
  return !ma_device_stop(&state.devices[type]);
}

float lovrAudioGetVolume() {
  float volume = 0.f;
  ma_device_get_master_volume(&state.devices[0], &volume);
  return volume;
}

void lovrAudioSetVolume(float volume) {
  ma_device_set_master_volume(&state.devices[0], volume);
}

// Source

Source* lovrSourceCreate(SoundData* sound) {
  Source* source = lovrAlloc(Source);
  source->sound = sound;
  lovrRetain(source->sound);
  source->volume = 1.f;

  for (size_t i = 0; i < state.converters.length; i++) {
    ma_data_converter* converter = &state.converters.data[i];
    if (converter->config.formatIn != formats[source->sound->format]) continue;
    if (converter->config.sampleRateIn != source->sound->sampleRate) continue;
    if (converter->config.channelsIn != source->sound->channels) continue;
    if (converter->config.channelsOut != (2 >> source->spatial)) continue;
    source->converter = converter;
    break;
  }

  if (!source->converter) {
    ma_data_converter_config config = ma_data_converter_config_init_default();
    config.formatIn = formats[source->sound->format];
    config.formatOut = ma_format_f32;
    config.channelsIn = source->sound->channels;
    config.channelsOut = 2 >> source->spatial;
    config.sampleRateIn = source->sound->sampleRate;
    config.sampleRateOut = SAMPLE_RATE;
    arr_expand(&state.converters, 1);
    ma_data_converter* converter = &state.converters.data[state.converters.length++];
    lovrAssert(!ma_data_converter_init(&config, converter), "Problem creating Source data converter");
    source->converter = converter;
  }

  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrRelease(SoundData, source->sound);
}

void lovrSourcePlay(Source* source) {
  ma_mutex_lock(&state.locks[AUDIO_PLAYBACK]);

  source->playing = true;

  if (!source->tracked) {
    lovrRetain(source);
    source->tracked = true;
    source->next = state.sources;
    state.sources = source;
  }

  ma_mutex_unlock(&state.locks[AUDIO_PLAYBACK]);
}

void lovrSourcePause(Source* source) {
  source->playing = false;
}

void lovrSourceStop(Source* source) {
  lovrSourcePause(source);
  lovrSourceSetTime(source, 0);
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
  ma_mutex_lock(&state.locks[AUDIO_PLAYBACK]);
  source->volume = volume;
  ma_mutex_unlock(&state.locks[AUDIO_PLAYBACK]);
}

uint32_t lovrSourceGetTime(Source* source) {
  return source->offset;
}

void lovrSourceSetTime(Source* source, uint32_t time) {
  ma_mutex_lock(&state.locks[AUDIO_PLAYBACK]);
  source->offset = time;
  ma_mutex_unlock(&state.locks[AUDIO_PLAYBACK]);
}

uint32_t lovrSourceGetDuration(Source* source) {
  return source->sound->frames;
}
