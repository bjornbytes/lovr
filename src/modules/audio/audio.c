#include "audio/audio.h"
#include "data/soundData.h"
#include "core/ref.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>
#include "lib/miniaudio/miniaudio.h"

struct Source {
  Source* next;
  SoundData* sound;
  uint32_t offset;
  float volume;
  bool playing;
  bool looping;
  bool tracked;
};

static struct {
  bool initialized;
  ma_context context;
  AudioConfig config[2];
  ma_device devices[2];
  ma_mutex locks[2];
  Source* sources;
} state;

static void onPlayback(ma_device* device, void* output, const void* input, uint32_t frames) {
  ma_mutex_lock(&state.locks[0]);

  Source* source = state.sources;

  for (;;) {
    if (!source) {
      break;
    }

    if (!source->playing) {

      continue;
    }
  }

  for (Source* source = state.sources; source != NULL; source = source->next) {
    if (!source->playing) {
      source->tracked = false;
      lovrRelease(Source, source);
      continue;
    }

    uint32_t n = source->sound->read(source->sound, source->offset, frames, output);

    if (n < frames) {
      source->offset = 0;
    } else {
      source->offset += n;
    }
  }

  ma_mutex_unlock(&state.locks[0]);
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t frames) {
  ma_mutex_lock(&state.locks[1]);
  ma_mutex_unlock(&state.locks[1]);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

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

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.devices[0]);
  ma_device_uninit(&state.devices[1]);
  ma_mutex_uninit(&state.locks[0]);
  ma_mutex_uninit(&state.locks[1]);
  ma_context_uninit(&state.context);
  memset(&state, 0, sizeof(state));
}

bool lovrAudioReset() {
  for (size_t i = 0; i < 2; i++) {
    if (state.config[i].enable) {
      ma_device_type deviceType = (i == 0) ? ma_device_type_playback : ma_device_type_capture;

      ma_device_config config = ma_device_config_init(deviceType);
      config.playback.format = ma_format_f32;
      config.playback.channels = 2;
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
  return 0; // TODO
}
