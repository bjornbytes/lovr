#include "audio/audio.h"
#include "audio/spatializer.h"
#include "data/sound.h"
#include "core/util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <intrin.h>
#define CTZL _tzcnt_u64
#else
#define CTZL __builtin_ctzl
#endif

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

#define FOREACH_SOURCE(s) for (uint64_t m = state.sourceMask; s = m ? state.sources[CTZL(m)] : NULL, m; m ^= (m & -m))
#define OUTPUT_FORMAT SAMPLE_F32
#define OUTPUT_CHANNELS 2
#define CAPTURE_CHANNELS 1
#define BUFFER_SIZE 256

struct Source {
  uint32_t ref;
  uint32_t index;
  Sound* sound;
  intptr_t spatializerMemo;
  uint32_t converter;
  uint32_t offset;
  float volume;
  float blend;
  float position[4];
  float orientation[4];
  float radius;
  float dipoleWeight;
  float dipolePower;
  uint8_t effects;
  bool playing;
  bool looping;
  bool spatial;
};

static struct {
  bool initialized;
  ma_context context;
  ma_device devices[2];
  ma_mutex lock;
  Source* sources[MAX_SOURCES];
  uint64_t sourceMask;
  Sound* captureStream;
  arr_t(ma_data_converter) converters;
  float position[4];
  float orientation[4];
  Spatializer* spatializer;
  uint32_t leftoverOffset;
  uint32_t leftoverFrames;
  float leftovers[BUFFER_SIZE * 2];
} state;

// Device callbacks

static void onPlayback(ma_device* device, void* out, const void* in, uint32_t count) {
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
      return;
    }
  }

  ma_mutex_lock(&state.lock);

  do {
    float raw[BUFFER_SIZE * 2];
    float aux[BUFFER_SIZE * 2];
    float mix[BUFFER_SIZE * 2];

    float* dst = count >= BUFFER_SIZE ? output : state.leftovers;
    float* src = NULL; // The "current" buffer (used for fast paths)

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
      uint32_t channels = source->spatial ? 1 : 2;
      uint64_t frameLimit = sizeof(raw) / lovrSoundGetChannelCount(source->sound) / sizeof(float);
      uint32_t framesToConvert = BUFFER_SIZE;
      uint32_t framesConverted = 0;
      while (framesToConvert > 0) {
        src = raw;
        ma_data_converter* converter = source->converter == ~0u ? NULL : &state.converters.data[source->converter];
        uint64_t framesToRead = converter ? MIN(ma_data_converter_get_required_input_frame_count(converter, framesToConvert), frameLimit) : framesToConvert;
        uint64_t framesRead = lovrSoundRead(source->sound, source->offset, framesToRead, src);
        ma_uint64 framesIn = framesRead;
        ma_uint64 framesOut = framesToConvert;

        if (framesRead == 0) {
          if (source->looping) {
            source->offset = 0;
            continue;
          } else {
            source->offset = 0;
            source->playing = false;
            memset(src + framesConverted * channels, 0, framesToConvert * channels * sizeof(float));
            break;
          }
        }

        if (converter) {
          ma_data_converter_process_pcm_frames(converter, src, &framesIn, aux + framesConverted * channels, &framesOut);
          src = aux;
        }

        framesToConvert -= framesOut;
        framesConverted += framesOut;
        source->offset += framesRead;
      }

      // Spatialize
      if (source->spatial) {
        state.spatializer->apply(source, src, mix, BUFFER_SIZE, BUFFER_SIZE);
        src = mix;
      }

      // Mix
      float volume = source->volume;
      for (uint32_t i = 0; i < OUTPUT_CHANNELS * BUFFER_SIZE; i++) {
        dst[i] += src[i] * volume;
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
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t count) {
  lovrSoundWrite(state.captureStream, 0, count, input);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer* spatializers[] = {
#ifdef LOVR_ENABLE_PHONON
  &phononSpatializer,
#endif
#ifdef LOVR_ENABLE_OCULUS_AUDIO
  &oculusSpatializer,
#endif
  &simpleSpatializer
};

// Entry

bool lovrAudioInit(const char* spatializer) {
  if (state.initialized) return false;

  if (ma_context_init(NULL, 0, NULL, &state.context)) {
    return false;
  }

  int mutexStatus = ma_mutex_init(&state.lock);
  lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");

  SpatializerConfig spatializerConfig = {
    .maxSourcesHint = MAX_SOURCES,
    .fixedBufferSize = BUFFER_SIZE,
    .sampleRate = PLAYBACK_SAMPLE_RATE
  };

  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (spatializer && strcmp(spatializer, spatializers[i]->name)) {
      continue;
    }

    if (spatializers[i]->init(spatializerConfig)) {
      state.spatializer = spatializers[i];
      break;
    }
  }
  lovrAssert(state.spatializer, "Must have at least one spatializer");

  arr_init(&state.converters, realloc);
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
  lovrRelease(state.captureStream, lovrSoundDestroy);
  if (state.spatializer) state.spatializer->destroy();
  for (size_t i = 0; i < state.converters.length; i++) {
    ma_data_converter_uninit(&state.converters.data[i]);
  }
  arr_free(&state.converters);
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

bool lovrAudioSetDevice(AudioType type, void* id, size_t size, uint32_t sampleRate, uint32_t format, bool exclusive) {
  if (id && size != sizeof(ma_device_id)) return false;

#ifdef ANDROID
  // XX<nevyn> miniaudio doesn't seem to be happy to set a specific device an android (fails with
  // error -2 on device init). Since there is only one playback and one capture device in OpenSL,
  // we can just set this to NULL and make this call a no-op.
  id = NULL;
#endif

  ma_device_config config;

  if (type == AUDIO_PLAYBACK) {
    sampleRate = sampleRate ? sampleRate : PLAYBACK_SAMPLE_RATE;
    lovrAssert(sampleRate == PLAYBACK_SAMPLE_RATE, "Playback sample rate must be %d", PLAYBACK_SAMPLE_RATE);
    lovrAssert(format == SAMPLE_F32, "Playback format must be f32");
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = (ma_device_id*) id;
    config.playback.format = miniaudioFormats[format];
    config.playback.channels = OUTPUT_CHANNELS;
    config.playback.shareMode = exclusive ? ma_share_mode_exclusive : ma_share_mode_shared;
  } else {
    sampleRate = sampleRate ? sampleRate : 16000;
    config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = (ma_device_id*) id;
    config.capture.format = miniaudioFormats[format];
    config.capture.channels = CAPTURE_CHANNELS;
    config.capture.shareMode = exclusive ? ma_share_mode_exclusive : ma_share_mode_shared;
    lovrRelease(state.captureStream, lovrSoundDestroy);
    state.captureStream = lovrSoundCreateStream(sampleRate * 1., format, CAPTURE_CHANNELS, sampleRate);
  }

  config.sampleRate = sampleRate;
  config.periodSizeInFrames = BUFFER_SIZE;
  config.dataCallback = callbacks[type];

  ma_device_uninit(&state.devices[type]);
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

float lovrAudioGetVolume() {
  float volume = 0.f;
  ma_device_get_master_volume(&state.devices[AUDIO_PLAYBACK], &volume);
  return volume;
}

void lovrAudioSetVolume(float volume) {
  ma_device_set_master_volume(&state.devices[AUDIO_PLAYBACK], volume);
}

void lovrAudioGetPose(float position[4], float orientation[4]) {
  memcpy(position, state.position, sizeof(state.position));
  memcpy(orientation, state.orientation, sizeof(state.orientation));
}

void lovrAudioSetPose(float position[4], float orientation[4]) {
  state.spatializer->setListenerPose(position, orientation);
}

bool lovrAudioSetGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material) {
  return state.spatializer->setGeometry(vertices, indices, vertexCount, indexCount, material);
}

const char* lovrAudioGetSpatializer() {
  return state.spatializer->name;
}

Sound* lovrAudioGetCaptureStream() {
  return state.captureStream;
}

// Source

Source* lovrSourceCreate(Sound* sound, bool spatial) {
  lovrAssert(lovrSoundGetChannelLayout(sound) != CHANNEL_AMBISONIC, "Ambisonic Sources are not currently supported");
  Source* source = calloc(1, sizeof(Source));
  lovrAssert(source, "Out of memory");
  source->ref = 1;
  source->index = ~0u;
  source->sound = sound;
  lovrRetain(source->sound);

  source->volume = 1.f;
  source->spatial = spatial;
  source->converter = ~0u;

  ma_data_converter_config config = ma_data_converter_config_init_default();
  config.formatIn = miniaudioFormats[lovrSoundGetFormat(sound)];
  config.formatOut = miniaudioFormats[OUTPUT_FORMAT];
  config.channelsIn = lovrSoundGetChannelCount(sound);
  config.channelsOut = spatial ? 1 : 2;
  config.sampleRateIn = lovrSoundGetSampleRate(sound);
  config.sampleRateOut = PLAYBACK_SAMPLE_RATE;

  if (config.formatIn != config.formatOut || config.channelsIn != config.channelsOut || config.sampleRateIn != config.sampleRateOut) {
    for (size_t i = 0; i < state.converters.length; i++) {
      ma_data_converter* converter = &state.converters.data[i];
      if (converter->config.formatIn != config.formatIn) continue;
      if (converter->config.sampleRateIn != config.sampleRateIn) continue;
      if (converter->config.channelsIn != config.channelsIn) continue;
      if (converter->config.channelsOut != config.channelsOut) continue;
      source->converter = i;
      break;
    }

    if (source->converter == ~0u) {
      arr_expand(&state.converters, 1);
      ma_data_converter* converter = &state.converters.data[state.converters.length];
      ma_result status = ma_data_converter_init(&config, converter);
      lovrAssert(status == MA_SUCCESS, "Problem creating Source data converter: %s (%d)", ma_result_description(status), status);
      source->converter = state.converters.length++;
    }
  }

  return source;
}

Source* lovrSourceClone(Source* source) {
  Source* clone = calloc(1, sizeof(Source));
  lovrAssert(clone, "Out of memory");
  clone->ref = 1;
  clone->sound = source->sound;
  lovrRetain(clone->sound);
  clone->volume = source->volume;
  clone->spatial = source->spatial;
  clone->converter = source->converter;
  return clone;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrRelease(source->sound, lovrSoundDestroy);
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

float lovrSourceGetVolume(Source* source) {
  return source->volume;
}

void lovrSourceSetVolume(Source* source, float volume) {
  source->volume = volume;
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

bool lovrSourceIsSpatial(Source *source) {
  return source->spatial;
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
  return source->effects & (1 << effect);
}

void lovrSourceSetEffectEnabled(Source* source, Effect effect, bool enabled) {
  if (enabled) {
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
