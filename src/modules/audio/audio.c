#include "audio/audio.h"
#include "audio/spatializer.h"
#include "data/sound.h"
#include "core/maf.h"
#include "core/util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifdef _MSC_VER
#include <intrin.h>
#define CTZL _tzcnt_u64
#else
#define CTZL __builtin_ctzl
#endif

#define FOREACH_SOURCE(s) for (uint64_t m = state.sourceMask; s = m ? state.sources[CTZL(m)] : NULL, m; m ^= (m & -m))
#define OUTPUT_FORMAT SAMPLE_F32
#define OUTPUT_CHANNELS 2

struct Source {
  uint32_t ref;
  uint32_t index;
  Sound* sound;
  ma_data_converter* converter;
  intptr_t spatializerMemo;
  uint32_t offset;
  float volume;
  float position[4];
  float orientation[4];
  float radius;
  float dipoleWeight;
  float dipolePower;
  uint8_t effects;
  bool playing;
  bool looping;
};

static struct {
  bool initialized;
  ma_mutex lock;
  ma_context context;
  ma_device devices[2];
  Sound* sinks[2];
  Source* sources[MAX_SOURCES];
  uint64_t sourceMask;
  float position[4];
  float orientation[4];
  Spatializer* spatializer;
  uint32_t leftoverOffset;
  uint32_t leftoverFrames;
  float leftovers[BUFFER_SIZE * 2];
  float absorption[3];
  ma_data_converter playbackConverter;
} state;

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

static float dbToLinear(float db) {
  return powf(10.f, db / 20.f);
}

static float linearToDb(float linear) {
  return 20.f * log10f(linear);
}

// Device callbacks

static void onPlayback(ma_device* device, void* out, const void* in, uint32_t count) {
  float raw[BUFFER_SIZE * 2];
  float aux[BUFFER_SIZE * 2];
  float mix[BUFFER_SIZE * 2];
  uint32_t total = count;
  float* output = out;

  // Consume any leftovers from the previous callback
  if (state.leftoverFrames > 0) {
    uint32_t leftoverFrames = MIN(count, state.leftoverFrames);
    memcpy(output, state.leftovers + state.leftoverOffset * OUTPUT_CHANNELS, leftoverFrames * OUTPUT_CHANNELS * sizeof(float));
    state.leftoverOffset += leftoverFrames;
    state.leftoverFrames -= leftoverFrames;
    output += leftoverFrames * OUTPUT_CHANNELS;
    count -= leftoverFrames;
    if (count == 0) {
      goto sink;
    }
  }

  ma_mutex_lock(&state.lock);

  do {
    float* dst = count >= BUFFER_SIZE ? output : state.leftovers;
    float* buf = NULL; // The "current" buffer (used for fast paths)

    if (dst == state.leftovers) {
      memset(dst, 0, sizeof(state.leftovers));
    }

    Source* source;
    FOREACH_SOURCE(source) {
      if (!source->playing) {
        state.sources[source->index] = NULL;
        state.sourceMask &= ~(1ull << source->index);
        state.spatializer->sourceDestroy(source);
        source->index = ~0u;
        lovrRelease(source, lovrSourceDestroy);
        continue;
      }

      // Read and convert raw frames until there's BUFFER_SIZE converted frames
      // - No converter: just read frames into raw (it has enough space for BUFFER_SIZE frames).
      // - Converter: keep reading as many frames as possible/needed into raw and convert into aux.
      // - If EOF is reached, rewind and continue for looping sources, otherwise pad end with zero.
      buf = source->converter ? aux : raw;
      float* cursor = buf; // Edge of processed frames
      uint32_t channelsOut = lovrSourceUsesSpatializer(source) ? 1 : 2; // If spatializer isn't converting to stereo, converter must do it
      uint32_t framesRemaining = BUFFER_SIZE;
      uint32_t framesProcessed = 0;
      while (framesRemaining > 0) {
        uint32_t framesRead;

        if (source->converter) {
          uint32_t channelsIn = lovrSoundGetChannelCount(source->sound);
          uint32_t capacity = sizeof(raw) / (channelsIn * sizeof(float));
          uint32_t chunk = MIN(ma_data_converter_get_required_input_frame_count(source->converter, framesRemaining), capacity);
          framesRead = lovrSoundRead(source->sound, source->offset, chunk, raw);
        } else {
          framesRead = lovrSoundRead(source->sound, source->offset, framesRemaining, cursor);
        }

        if (framesRead == 0) {
          if (source->looping) {
            source->offset = 0;
            continue;
          } else {
            source->offset = 0;
            source->playing = false;
            memset(cursor, 0, framesRemaining * channelsOut * sizeof(float));
            break;
          }
        } else {
          source->offset += framesRead;
        }

        if (source->converter) {
          ma_uint64 framesIn = framesRead;
          ma_uint64 framesOut = framesRemaining;
          ma_data_converter_process_pcm_frames(source->converter, raw, &framesIn, cursor, &framesOut);
          cursor += framesOut * channelsOut;
          framesProcessed += framesOut;
          framesRemaining -= framesOut;
        } else {
          cursor += framesRead * channelsOut;
          framesProcessed += framesRead;
          framesRemaining -= framesRead;
        }
      }

      // Spatialize
      if (lovrSourceUsesSpatializer(source)) {
        state.spatializer->apply(source, buf, mix, BUFFER_SIZE, BUFFER_SIZE);
        buf = mix;
      }

      // Mix
      float volume = source->volume;
      for (uint32_t i = 0; i < OUTPUT_CHANNELS * BUFFER_SIZE; i++) {
        dst[i] += buf[i] * volume;
      }
    }

    // Tail
    uint32_t tailCount = state.spatializer->tail(aux, mix, BUFFER_SIZE);
    for (uint32_t i = 0; i < tailCount * OUTPUT_CHANNELS; i++) {
      dst[i] += mix[i];
    }

    // Copy some leftovers to output
    if (dst == state.leftovers) {
      memcpy(output, state.leftovers, count * OUTPUT_CHANNELS * sizeof(float));
      state.leftoverFrames = BUFFER_SIZE - count;
      state.leftoverOffset = count;
    }

    output += BUFFER_SIZE * OUTPUT_CHANNELS;
    count -= MIN(count, BUFFER_SIZE);
  } while (count > 0);

  ma_mutex_unlock(&state.lock);

sink:
  if (state.sinks[AUDIO_PLAYBACK]) {
    uint64_t remaining = total;
    uint64_t capacity = sizeof(aux) / lovrSoundGetChannelCount(state.sinks[AUDIO_PLAYBACK]) / sizeof(float);
    output = out;

    while (remaining > 0) {
      ma_uint64 framesConsumed = remaining;
      ma_uint64 framesWritten = capacity;
      ma_data_converter_process_pcm_frames(&state.playbackConverter, output, &framesConsumed, aux, &framesWritten);
      lovrSoundWrite(state.sinks[AUDIO_PLAYBACK], 0, framesWritten, aux);
      output += framesConsumed * OUTPUT_CHANNELS;
      remaining -= framesConsumed;
    }
  }
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t count) {
  lovrSoundWrite(state.sinks[AUDIO_CAPTURE], 0, count, input);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer* spatializers[] = {
#ifdef LOVR_ENABLE_PHONON_SPATIALIZER
  &phononSpatializer,
#endif
#ifdef LOVR_ENABLE_OCULUS_SPATIALIZER
  &oculusSpatializer,
#endif
  &simpleSpatializer
};

// Entry

bool lovrAudioInit(const char* spatializer) {
  if (state.initialized) return false;

  ma_result result = ma_context_init(NULL, 0, NULL, &state.context);
  lovrAssert(result == MA_SUCCESS, "Failed to initialize miniaudio");

  result = ma_mutex_init(&state.lock);
  lovrAssert(result == MA_SUCCESS, "Failed to create audio mutex");

  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (spatializer && strcmp(spatializer, spatializers[i]->name)) {
      continue;
    }

    if (spatializers[i]->init()) {
      state.spatializer = spatializers[i];
      break;
    }
  }
  lovrAssert(state.spatializer, "Must have at least one spatializer");

  // SteamAudio's default frequency-dependent absorption coefficients for air
  state.absorption[0] = .0002f;
  state.absorption[1] = .0017f;
  state.absorption[1] = .0182f;

  quat_identity(state.orientation);

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < 2; i++) {
    ma_device_uninit(&state.devices[i]);
  }
  Source* source;
  FOREACH_SOURCE(source) lovrRelease(source, lovrSourceDestroy);
  ma_mutex_uninit(&state.lock);
  ma_context_uninit(&state.context);
  lovrRelease(state.sinks[AUDIO_PLAYBACK], lovrSoundDestroy);
  lovrRelease(state.sinks[AUDIO_CAPTURE], lovrSoundDestroy);
  if (state.spatializer) state.spatializer->destroy();
  ma_data_converter_uninit(&state.playbackConverter);
  memset(&state, 0, sizeof(state));
}

static AudioDeviceCallback* enumerateCallback;

static ma_bool32 enumPlayback(ma_context* context, ma_device_type type, const ma_device_info* info, void* userdata) {
  if (type == ma_device_type_playback) enumerateCallback(&info->id, sizeof(info->id), info->name, info->isDefault, userdata);
  return MA_TRUE;
}

static ma_bool32 enumCapture(ma_context* context, ma_device_type type, const ma_device_info* info, void* userdata) {
  if (type == ma_device_type_capture) enumerateCallback(&info->id, sizeof(info->id), info->name, info->isDefault, userdata);
  return MA_TRUE;
}

void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata) {
  enumerateCallback = callback;
  ma_context_enumerate_devices(&state.context, type == AUDIO_PLAYBACK ? enumPlayback : enumCapture, userdata);
}

bool lovrAudioSetDevice(AudioType type, void* id, size_t size, Sound* sink, AudioShareMode shareMode) {
  if (id && size != sizeof(ma_device_id)) return false;

  // If no sink is provided for a capture device, one is created internally
  if (type == AUDIO_CAPTURE && !sink) {
    sink = lovrSoundCreateStream(SAMPLE_RATE * 1., SAMPLE_F32, CHANNEL_MONO, SAMPLE_RATE);
  } else {
    lovrRetain(sink);
  }

  lovrAssert(!sink || lovrSoundGetChannelLayout(sink) != CHANNEL_AMBISONIC, "Ambisonic Sounds cannot be used as sinks");
  lovrAssert(!sink || lovrSoundIsStream(sink), "Sinks must be streams");

  ma_device_uninit(&state.devices[type]);
  lovrRelease(state.sinks[type], lovrSoundDestroy);
  state.sinks[type] = sink;

#ifdef ANDROID
  // XXX<nevyn> miniaudio doesn't seem to be happy to set a specific device an android (fails with
  // error -2 on device init). Since there is only one playback and one capture device in OpenSL,
  // we can just set this to NULL and make this call a no-op.
  id = NULL;
#endif

  static const ma_share_mode shareModes[] = {
    [AUDIO_SHARED] = ma_share_mode_shared,
    [AUDIO_EXCLUSIVE] = ma_share_mode_exclusive
  };

  ma_device_config config;

  if (type == AUDIO_PLAYBACK) {
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = (ma_device_id*) id;
    config.playback.shareMode = shareModes[shareMode];
    config.playback.format = ma_format_f32;
    config.playback.channels = OUTPUT_CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    if (sink) {
      ma_data_converter_config converterConfig = ma_data_converter_config_init_default();
      converterConfig.formatIn = config.playback.format;
      converterConfig.formatOut = miniaudioFormats[lovrSoundGetFormat(sink)];
      converterConfig.channelsIn = config.playback.channels;
      converterConfig.channelsOut = lovrSoundGetChannelCount(sink);
      converterConfig.sampleRateIn = config.sampleRate;
      converterConfig.sampleRateOut = lovrSoundGetSampleRate(sink);
      if (memcmp(&converterConfig, &state.playbackConverter.config, sizeof(ma_data_converter_config))) {
        ma_data_converter_uninit(&state.playbackConverter);
        ma_result status = ma_data_converter_init(&converterConfig, &state.playbackConverter);
        lovrAssert(status == MA_SUCCESS, "Failed to create sink data converter");
      }
    }
  } else {
    config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = (ma_device_id*) id;
    config.capture.shareMode = shareModes[shareMode];
    config.capture.format = miniaudioFormats[lovrSoundGetFormat(sink)];
    config.capture.channels = lovrSoundGetChannelCount(sink);
    config.sampleRate = lovrSoundGetSampleRate(sink);
  }

#ifndef EMSCRIPTEN // Web needs to use the default bigger buffer size to prevent stutters
  config.periodSizeInFrames = BUFFER_SIZE;
#endif
  config.dataCallback = callbacks[type];

  ma_result result = ma_device_init(&state.context, &config, &state.devices[type]);
  return result == MA_SUCCESS;
}

bool lovrAudioStart(AudioType type) {
  return ma_device_start(&state.devices[type]) == MA_SUCCESS;
}

bool lovrAudioStop(AudioType type) {
  return ma_device_stop(&state.devices[type]) == MA_SUCCESS;
}

bool lovrAudioIsStarted(AudioType type) {
  return ma_device_is_started(&state.devices[type]);
}

float lovrAudioGetVolume(VolumeUnit units) {
  float volume = 0.f;
  ma_device_get_master_volume(&state.devices[AUDIO_PLAYBACK], &volume);
  return units == UNIT_LINEAR ? volume : linearToDb(volume);
}

void lovrAudioSetVolume(float volume, VolumeUnit units) {
  if (units == UNIT_DECIBELS) volume = dbToLinear(volume);
  ma_device_set_master_volume(&state.devices[AUDIO_PLAYBACK], CLAMP(volume, 0.f, 1.f));
}

void lovrAudioGetPose(float position[4], float orientation[4]) {
  memcpy(position, state.position, sizeof(state.position));
  memcpy(orientation, state.orientation, sizeof(state.orientation));
}

void lovrAudioSetPose(float position[4], float orientation[4]) {
  state.spatializer->setListenerPose(position, orientation);
}

bool lovrAudioSetGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material) {
  ma_mutex_lock(&state.lock);
  bool success = state.spatializer->setGeometry(vertices, indices, vertexCount, indexCount, material);
  ma_mutex_unlock(&state.lock);
  return success;
}

const char* lovrAudioGetSpatializer() {
  return state.spatializer->name;
}

void lovrAudioGetAbsorption(float absorption[3]) {
  memcpy(absorption, state.absorption, 3 * sizeof(float));
}

void lovrAudioSetAbsorption(float absorption[3]) {
  ma_mutex_lock(&state.lock);
  memcpy(state.absorption, absorption, 3 * sizeof(float));
  ma_mutex_unlock(&state.lock);
}

// Source

Source* lovrSourceCreate(Sound* sound, uint32_t effects) {
  lovrAssert(lovrSoundGetChannelLayout(sound) != CHANNEL_AMBISONIC, "Ambisonic Sources are not currently supported");
  Source* source = calloc(1, sizeof(Source));
  lovrAssert(source, "Out of memory");
  source->ref = 1;
  source->index = ~0u;
  source->sound = sound;
  lovrRetain(source->sound);

  source->volume = 1.f;
  source->effects = effects;
  quat_identity(source->orientation);

  ma_data_converter_config config = ma_data_converter_config_init_default();
  config.formatIn = miniaudioFormats[lovrSoundGetFormat(sound)];
  config.formatOut = miniaudioFormats[OUTPUT_FORMAT];
  config.channelsIn = lovrSoundGetChannelCount(sound);
  config.channelsOut = lovrSourceUsesSpatializer(source) ? 1 : 2; // See onPlayback
  config.sampleRateIn = lovrSoundGetSampleRate(sound);
  config.sampleRateOut = SAMPLE_RATE;

  if (config.formatIn != config.formatOut || config.channelsIn != config.channelsOut || config.sampleRateIn != config.sampleRateOut) {
    source->converter = malloc(sizeof(ma_data_converter));
    lovrAssert(source->converter, "Out of memory");
    ma_result status = ma_data_converter_init(&config, source->converter);
    lovrAssert(status == MA_SUCCESS, "Problem creating Source data converter: %s (%d)", ma_result_description(status), status);
  }

  return source;
}

Source* lovrSourceClone(Source* source) {
  Source* clone = calloc(1, sizeof(Source));
  lovrAssert(clone, "Out of memory");
  clone->ref = 1;
  clone->index = ~0u;
  clone->sound = source->sound;
  lovrRetain(clone->sound);
  clone->volume = source->volume;
  memcpy(clone->position, source->position, 4 * sizeof(float));
  memcpy(clone->orientation, source->orientation, 4 * sizeof(float));
  clone->radius = source->radius;
  clone->dipoleWeight = source->dipoleWeight;
  clone->dipolePower = source->dipolePower;
  clone->effects = source->effects;
  clone->looping = source->looping;
  if (source->converter) {
    clone->converter = malloc(sizeof(ma_data_converter));
    lovrAssert(clone->converter, "Out of memory");
    ma_result status = ma_data_converter_init(&source->converter->config, clone->converter);
    lovrAssert(status == MA_SUCCESS, "Problem creating Source data converter: %s (%d)", ma_result_description(status), status);
  }
  return clone;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrRelease(source->sound, lovrSoundDestroy);
  ma_data_converter_uninit(source->converter);
  free(source->converter);
  free(source);
}

Sound* lovrSourceGetSound(Source* source) {
  return source->sound;
}

bool lovrSourcePlay(Source* source) {
  if (state.sourceMask == ~0ull) {
    return false;
  }

  ma_mutex_lock(&state.lock);

  source->playing = true;

  // If the source isn't tracked, set its index to the right-most zero bit in the mask
  if (source->index == ~0u) {
    uint32_t index = state.sourceMask ? CTZL(~state.sourceMask) : 0;
    state.sourceMask |= (1ull << index);
    state.sources[index] = source;
    source->index = index;
    lovrRetain(source);
    state.spatializer->sourceCreate(source);
  }

  ma_mutex_unlock(&state.lock);
  return true;
}

void lovrSourcePause(Source* source) {
  source->playing = false;
}

void lovrSourceStop(Source* source) {
  lovrSourcePause(source);
  lovrSourceSeek(source, 0, UNIT_FRAMES);
}

bool lovrSourceIsPlaying(Source* source) {
  return source->playing;
}

bool lovrSourceIsLooping(Source* source) {
  return source->looping;
}

void lovrSourceSetLooping(Source* source, bool loop) {
  lovrAssert(loop == false || lovrSoundIsStream(source->sound) == false, "Can't loop streams");
  source->looping = loop;
}

float lovrSourceGetVolume(Source* source, VolumeUnit units) {
  return units == UNIT_LINEAR ? source->volume : linearToDb(source->volume);
}

void lovrSourceSetVolume(Source* source, float volume, VolumeUnit units) {
  if (units == UNIT_DECIBELS) volume = dbToLinear(volume);
  source->volume = CLAMP(volume, 0.f, 1.f);
}

void lovrSourceSeek(Source* source, double time, TimeUnit units) {
  ma_mutex_lock(&state.lock);
  source->offset = units == UNIT_SECONDS ? (uint32_t) (time * lovrSoundGetSampleRate(source->sound) + .5) : (uint32_t) time;
  ma_mutex_unlock(&state.lock);
}

double lovrSourceTell(Source* source, TimeUnit units) {
  return units == UNIT_SECONDS ? (double) source->offset / lovrSoundGetSampleRate(source->sound) : source->offset;
}

double lovrSourceGetDuration(Source* source, TimeUnit units) {
  uint32_t frames = lovrSoundGetFrameCount(source->sound);
  return units == UNIT_SECONDS ? (double) frames / lovrSoundGetSampleRate(source->sound) : frames;
}

bool lovrSourceUsesSpatializer(Source* source) {
  return source->effects != EFFECT_NONE; // Currently, all effects require the spatializer
}

void lovrSourceGetPose(Source *source, float position[4], float orientation[4]) {
  memcpy(position, source->position, sizeof(source->position));
  memcpy(orientation, source->orientation, sizeof(source->orientation));
}

void lovrSourceSetPose(Source *source, float position[4], float orientation[4]) {
  ma_mutex_lock(&state.lock);
  memcpy(source->position, position, sizeof(source->position));
  memcpy(source->orientation, orientation, sizeof(source->orientation));
  ma_mutex_unlock(&state.lock);
}

float lovrSourceGetRadius(Source* source) {
  return source->radius;
}

void lovrSourceSetRadius(Source* source, float radius) {
  source->radius = radius;
}

void lovrSourceGetDirectivity(Source* source, float* weight, float* power) {
  *weight = source->dipoleWeight;
  *power = source->dipolePower;
}

void lovrSourceSetDirectivity(Source* source, float weight, float power) {
  source->dipoleWeight = weight;
  source->dipolePower = power;
}

bool lovrSourceIsEffectEnabled(Source* source, Effect effect) {
  return source->effects == EFFECT_NONE ? false : (source->effects & (1 << effect));
}

void lovrSourceSetEffectEnabled(Source* source, Effect effect, bool enabled) {
  if (enabled && source->effects != EFFECT_NONE) {
    source->effects |= (1 << effect);
  } else {
    source->effects &= ~(1 << effect);
  }
}

intptr_t* lovrSourceGetSpatializerMemoField(Source* source) {
  return &source->spatializerMemo;
}

uint32_t lovrSourceGetIndex(Source* source) {
  return source->index;
}
