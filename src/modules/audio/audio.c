#include "audio/audio.h"
#include "data/blob.h"
#include "data/sound.h"
#include "core/job.h"
#include "core/maf.h"
#include "util.h"
#include "lib/miniaudio/miniaudio.h"
#ifdef LOVR_USE_PHONON
#include <phonon.h>
#endif
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _MSC_VER
#include <intrin.h>
#define CTZL _tzcnt_u64
#else
#define CTZL __builtin_ctzl
#endif

#define FOREACH_SOURCE(mask, s) for (uint64_t m = mask; s = m ? state.activeSources[CTZL(m)] : NULL, m; m ^= (m & -m))
#define AMBISONIC_ORDER(channels) ((channels / 6) + 1)
#define OUTPUT_FORMAT SAMPLE_F32
#define MAX_OCCLUSION_SAMPLES 64
#define NO_HRTF ((IPLHRTF) (uintptr_t) ~0ull)
#define BUFFER_SIZE 256
#define MAX_SOURCES 64

struct Source {
  atomic_uint ref;
  uint32_t slot;
  Sound* sound;
  ma_data_converter* converter;
  float pitchRatio;
  float volume;
  float position[3];
  float radius;
  float orientation[4];
  float absorption[3];
  float innerAngle;
  float outerAngle;
  float outerAngleVolume;
  float innerDistance;
  float minFalloffVolume;
  uint32_t occlusionRays;
  uint32_t transmissionRays;
  float reverb;
  ReverbMode reverbMode;
  float spatialization;
  bool looping;
  bool pitchable;
  bool spatial;
  atomic_bool playing;
  atomic_bool hasTail;
  atomic_uint offset;
  atomic_uint playRequest;
  atomic_uint seekRequest;
#ifdef LOVR_USE_PHONON
  IPLSource handle;
  IPLDirectEffect directEffect;
  IPLPanningEffect panningEffect;
  IPLBinauralEffect binauralEffect;
  IPLReflectionEffect reflectionEffect;
  IPLAmbisonicsDecodeEffect ambisonicEffect;
  IPLAudioBuffer ambisonicBuffer;
  IPLDirectEffectParams directParams[2];
  IPLVector3 relativeDirection[2];
#endif
};

struct AudioMesh {
  uint32_t ref;
  bool enabled;
  AudioMesh* parent;
  float transform[16];
#ifdef LOVR_USE_PHONON
  IPLInstancedMesh instancedMesh;
  IPLStaticMesh staticMesh;
  IPLScene scene;
#endif
};

static atomic_uint ref;

static struct {
  AudioConfig config;
  ma_log log;
  ma_context context;
  ma_device devices[2];
  ma_device_info* deviceInfo[2];
  ma_data_converter playbackConverter;
  AudioStream* streams[2];
  Source* activeSources[64];
  atomic_ullong activeSourceMask;
  uint64_t pendingSourceMask;
  atomic_uint backbuffer;
  uint32_t frontbuffer;
  bool simulatorDirty;
  float position[3];
  float orientation[4];
#ifdef LOVR_USE_PHONON
  IPLContext phonon;
  IPLAudioSettings audioSettings;
  IPLSimulator simulator;
  IPLSource listener;
  IPLScene scene;
  bool sceneDirty;
  atomic_uint enabledMeshCount;
  _Atomic(IPLHRTF) hrtf[2];
  IPLCoordinateSpace3 listenerBasis[2];
  IPLReflectionEffect reflectionEffect;
  IPLReflectionMixer reflectionMixer;
  IPLAmbisonicsDecodeEffect ambisonicsDecodeEffect;
  atomic_bool reverbFinished;
  atomic_ullong sourceReverbMask;
  atomic_ullong listenerReverbMask;
  IPLAudioBuffer listenerReverb;
  IPLAudioBuffer parametricReverb;
  float reverbSimulationTimer;
  float reverbDecay;
#endif
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

static bool phonon_init(void);
static void phonon_destroy(void);
static void phonon_update(float dt);
static bool phonon_set_hrtf(Blob* blob);
static void phonon_mix_begin(void);
static bool phonon_mix_source(Source* source, float* src, float* dst);
static void phonon_mix_reverb(float* dst);
static bool phonon_source_init(Source* source);
static void phonon_source_destroy(Source* source);
static void phonon_source_reset(Source* source);
static void phonon_source_add(Source* source);
static void phonon_source_remove(Source* source);
static bool phonon_mesh_init(AudioMesh* mesh, float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial* materials, AudioMaterial material);
static bool phonon_mesh_init_clone(AudioMesh* clone);
static void phonon_mesh_destroy(AudioMesh* mesh);
static void phonon_mesh_set_enabled(AudioMesh* mesh, bool enable);
static void phonon_mesh_set_transform(AudioMesh* mesh, float* transform);

// Device callbacks

static void onPlayback(ma_device* device, void* out, const void* in, uint32_t count) {
  float raw[BUFFER_SIZE * MAX_CHANNELS];
  float tmp[BUFFER_SIZE * MAX_CHANNELS];
  float* buf = NULL;
  float* dst = out;
  Source* source;

  phonon_mix_begin();

  FOREACH_SOURCE(state.activeSourceMask, source) {
    if (!source) {
      continue;
    }

    uint32_t play = atomic_exchange(&source->playRequest, ~0u);

    if (play != ~0u) {
      if (!source->playing && play == 1) {
        phonon_source_reset(source);
      }

      source->playing = !!play;
    }

    uint32_t seek = atomic_exchange(&source->seekRequest, ~0u);
    if (seek != ~0u) source->offset = seek;

    if (source->pitchable) {
      ma_data_converter_set_rate_ratio(source->converter, source->pitchRatio);
    }

    uint32_t channels = lovrSoundGetChannelCount(source->sound);

    if (source->playing) {
      // Read and convert raw frames until there's BUFFER_SIZE converted frames
      // - No converter: just read frames into raw
      // - Converter: keep reading as many frames as possible/needed into raw and convert into tmp.
      // - If EOF is reached, rewind and continue for looping sources, otherwise pad end with zero.
      float* cursor = source->converter ? tmp : raw; // Edge of processed frames
      uint32_t framesRemaining = BUFFER_SIZE;

      while (framesRemaining > 0) {
        uint32_t framesRead;

        if (source->converter) {
          uint32_t capacity = sizeof(raw) / (channels * sizeof(float));
          ma_uint64 chunk;
          ma_data_converter_get_required_input_frame_count(source->converter, framesRemaining, &chunk);
          framesRead = lovrSoundRead(source->sound, source->offset, MIN(chunk, capacity), raw);
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
            memset(cursor, 0, framesRemaining * channels * sizeof(float));
            break;
          }
        } else {
          uint32_t offset = atomic_load_explicit(&source->offset, memory_order_relaxed);
          atomic_store_explicit(&source->offset, offset + framesRead, memory_order_relaxed);
        }

        if (source->converter) {
          ma_uint64 framesIn = framesRead;
          ma_uint64 framesOut = framesRemaining;
          ma_data_converter_process_pcm_frames(source->converter, raw, &framesIn, cursor, &framesOut);
          cursor += framesOut * channels;
          framesRemaining -= framesOut;
        } else {
          cursor += framesRead * channels;
          framesRemaining -= framesRead;
        }
      }

      buf = source->converter ? tmp : raw;

      for (uint32_t i = 0; i < channels * BUFFER_SIZE; i++) {
        buf[i] *= source->volume;
      }
    } else {
      memset(raw, 0, BUFFER_SIZE * channels * sizeof(float));
      buf = raw;
    }

    float* mix = buf == raw ? tmp : raw;

    // Spatialize
    if (source->spatial) {
      source->hasTail = phonon_mix_source(source, buf, mix);
      buf = mix;
    } else if (channels == 1) {
      for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        mix[i * 2 + 0] = buf[i];
        mix[i * 2 + 1] = buf[i];
      }
      buf = mix;
    }

    // Mix
    for (uint32_t i = 0; i < 2 * BUFFER_SIZE; i++) {
      dst[i] += buf[i];
    }
  }

  if (state.config.reverb.type != REVERB_NONE) {
    phonon_mix_reverb(dst);
  }

  if (state.streams[AUDIO_PLAYBACK]) {
    Sound* sound = lovrAudioStreamGetSound(state.streams[AUDIO_PLAYBACK]);
    uint64_t capacity = sizeof(tmp) / lovrSoundGetChannelCount(sound) / sizeof(float);
    while (count > 0) {
      ma_uint64 framesConsumed = count;
      ma_uint64 framesWritten = capacity;
      ma_data_converter_process_pcm_frames(&state.playbackConverter, dst, &framesConsumed, tmp, &framesWritten);
      if (lovrAudioStreamWrite(state.streams[AUDIO_PLAYBACK], framesWritten, tmp) < framesWritten) break;
      dst += framesConsumed * 2;
      count -= framesConsumed;
    }
  }
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t count) {
  lovrAudioStreamWrite(state.streams[AUDIO_CAPTURE], count, input);
}

static void onLog(void* userdata, ma_uint32 maLevel, const char* message) {
  int level;
  switch (maLevel) {
    case MA_LOG_LEVEL_DEBUG: level = LOG_DEBUG; break;
    case MA_LOG_LEVEL_INFO: level = LOG_INFO; break;
    case MA_LOG_LEVEL_WARNING: level = LOG_WARN; break;
    case MA_LOG_LEVEL_ERROR: level = LOG_ERROR; break;
  }
  lovrLog(level, "MA", message);
}

// Entry

bool lovrAudioInit(AudioConfig* config) {
  if (!lovrModuleAcquire(&ref)) return true;

  state.config = *config;

  ma_context_config contextConfig = ma_context_config_init();

  if (config->debug) {
    ma_log_init(NULL, &state.log);
    ma_log_register_callback(&state.log, ma_log_callback_init(onLog, NULL));
    contextConfig.pLog = &state.log;
  }

  ma_result result = ma_context_init(NULL, 0, &contextConfig, &state.context);
  if (result != MA_SUCCESS) {
    lovrModuleReset(&ref);
    return lovrSetError("Failed to initialize miniaudio context: %s", ma_result_description(result));
  }

  if (!phonon_init()) {
    lovrAudioDestroy();
    return false;
  }

  quat_identity(state.orientation);
  lovrModuleReady(&ref);
  return true;
}

void lovrAudioDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  for (size_t i = 0; i < 2; i++) {
    ma_device_uninit(&state.devices[i]);
    lovrFree(state.deviceInfo[i]);
  }
  Source* source;
  FOREACH_SOURCE(state.activeSourceMask | state.pendingSourceMask, source) lovrRelease(source, lovrSourceDestroy);
  ma_context_uninit(&state.context);
  lovrRelease(state.streams[AUDIO_PLAYBACK], lovrAudioStreamDestroy);
  lovrRelease(state.streams[AUDIO_CAPTURE], lovrAudioStreamDestroy);
  ma_data_converter_uninit(&state.playbackConverter, NULL);
  phonon_destroy();
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

uint32_t lovrAudioGetSampleRate(void) {
  return state.config.sampleRate;
}

static AudioDeviceCallback* enumerateCallback;

static ma_bool32 enumPlayback(ma_context* context, ma_device_type type, const ma_device_info* info, void* userdata) {
  AudioDevice device = { sizeof(info->id), &info->id, info->name, info->isDefault };
  if (type == ma_device_type_playback) enumerateCallback(&device, userdata);
  return MA_TRUE;
}

static ma_bool32 enumCapture(ma_context* context, ma_device_type type, const ma_device_info* info, void* userdata) {
  AudioDevice device = { sizeof(info->id), &info->id, info->name, info->isDefault };
  if (type == ma_device_type_capture) enumerateCallback(&device, userdata);
  return MA_TRUE;
}

void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata) {
  enumerateCallback = callback;
  ma_context_enumerate_devices(&state.context, type == AUDIO_PLAYBACK ? enumPlayback : enumCapture, userdata);
}

bool lovrAudioGetDevice(AudioType type, AudioDevice* device) {
  if (!state.devices[type].pContext) {
    return false;
  }

  if (!state.deviceInfo[type]) {
    state.deviceInfo[type] = lovrMalloc(sizeof(ma_device_info));
  }

  ma_device_info* info = state.deviceInfo[type];
  ma_device_type deviceType = type == AUDIO_PLAYBACK ? ma_device_type_playback : ma_device_type_capture;
  ma_result result = ma_device_get_info(&state.devices[type], deviceType, info);
  device->idSize = sizeof(ma_device_id);
  device->id = &info->id;
  device->name = info->name;
  device->isDefault = info->isDefault;
  return result == MA_SUCCESS;
}

bool lovrAudioSetDevice(AudioType type, void* id, size_t size, bool read, AudioStream* stream, AudioShareMode shareMode) {
  lovrAssert(!id || size == sizeof(ma_device_id), "Invalid device ID");
  lovrCheck(!stream || lovrSoundGetChannelCount(lovrAudioStreamGetSound(stream)) <= 2, "Stream must be mono or stereo");

  ma_device_uninit(&state.devices[type]);
  lovrRelease(state.streams[type], lovrAudioStreamDestroy);
  state.streams[type] = NULL;

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

  ma_result result;
  ma_device_config config;

  if (type == AUDIO_PLAYBACK) {
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = (ma_device_id*) id;
    config.playback.shareMode = shareModes[shareMode];
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = state.config.sampleRate;
    config.periodSizeInFrames = BUFFER_SIZE;
    config.dataCallback = onPlayback;
    if (read) {
      if (stream) {
        Sound* sound = lovrAudioStreamGetSound(stream);
        ma_data_converter_config converterConfig = ma_data_converter_config_init_default();
        converterConfig.formatIn = config.playback.format;
        converterConfig.formatOut = miniaudioFormats[lovrSoundGetFormat(sound)];
        converterConfig.channelsIn = config.playback.channels;
        converterConfig.channelsOut = lovrSoundGetChannelCount(sound);
        converterConfig.sampleRateIn = config.sampleRate;
        converterConfig.sampleRateOut = lovrSoundGetSampleRate(sound);
        ma_data_converter_uninit(&state.playbackConverter, NULL);
        result = ma_data_converter_init(&converterConfig, NULL, &state.playbackConverter);
        lovrAssert(result == MA_SUCCESS, "Failed to create sink data converter: %s", ma_result_description(result));
        lovrRetain(stream);
      } else {
        stream = lovrAudioStreamCreate(state.config.sampleRate, SAMPLE_F32, 2, state.config.sampleRate);
      }
    }
  } else {
    config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = (ma_device_id*) id;
    config.capture.shareMode = shareModes[shareMode];
    config.periodSizeInFrames = BUFFER_SIZE;
    config.dataCallback = onCapture;

    if (stream) {
      Sound* sound = lovrAudioStreamGetSound(stream);
      config.capture.format = miniaudioFormats[lovrSoundGetFormat(sound)];
      config.capture.channels = lovrSoundGetChannelCount(sound);
      config.sampleRate = lovrSoundGetSampleRate(sound);
      lovrRetain(stream);
    } else {
      config.capture.format = ma_format_f32;
      config.capture.channels = 1;
    }
  }

  result = ma_device_init(&state.context, &config, &state.devices[type]);

  if (result != MA_SUCCESS) {
    lovrRelease(stream, lovrAudioStreamDestroy);
    return lovrSetError("Failed to initialize device: %s", ma_result_description(result));
  }

  if (type == AUDIO_CAPTURE && !stream) {
    uint32_t sampleRate = state.devices[AUDIO_CAPTURE].sampleRate;
    stream = lovrAudioStreamCreate(sampleRate, SAMPLE_F32, 1, sampleRate);
  }

  state.streams[type] = stream;
  return true;
}

AudioStream* lovrAudioGetStream(AudioType type) {
  return state.streams[type];
}

bool lovrAudioStart(AudioType type) {
  if (!state.devices[type].pContext) {
    return lovrSetError("no device");
  }

  ma_result result = ma_device_start(&state.devices[type]);
  lovrAssert(result == MA_SUCCESS, ma_result_description(result));
  return true;
}

bool lovrAudioStop(AudioType type) {
  ma_result result = ma_device_stop(&state.devices[type]);
  lovrAssert(result == MA_SUCCESS, ma_result_description(result));
  return true;
}

bool lovrAudioIsStarted(AudioType type) {
  return ma_device_is_started(&state.devices[type]);
}

void lovrAudioUpdate(float dt) {
  Source* source;
  FOREACH_SOURCE(state.activeSourceMask | state.pendingSourceMask, source) {
    if (!source->playing && source->playRequest != 1 && !source->hasTail) {
      phonon_source_remove(source);
      state.activeSourceMask &= ~(1ull << source->slot);
      state.pendingSourceMask &= ~(1ull << source->slot);
      state.activeSources[source->slot] = NULL;
      source->slot = ~0u;
      lovrRelease(source, lovrSourceDestroy);
    }
  }

  phonon_update(dt);

  atomic_fetch_or(&state.activeSourceMask, state.pendingSourceMask);
  state.pendingSourceMask = 0;
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

void lovrAudioGetPose(float position[3], float orientation[4]) {
  vec3_init(position, state.position);
  quat_init(orientation, state.orientation);
}

void lovrAudioSetPose(float position[3], float orientation[4]) {
  vec3_init(state.position, position);
  quat_init(state.orientation, orientation);
  state.simulatorDirty = true;
}

bool lovrAudioSetHRTF(Blob* blob) {
  return phonon_set_hrtf(blob);
}

// Source

Source* lovrSourceCreate(Sound* sound, bool pitchable, bool spatial) {
  lovrCheck(lovrSoundGetChannelCount(sound) <= 2 || spatial, "Ambisonic Sources must be spatial");

  Source* source = lovrCalloc(sizeof(Source));
  source->ref = 1;
  source->slot = ~0u;
  source->sound = sound;
  source->pitchRatio = ((float) lovrSoundGetSampleRate(source->sound) / state.config.sampleRate);
  source->volume = 1.f;
  quat_identity(source->orientation);
  source->outerAngleVolume = 1.f;
  source->minFalloffVolume = 1.f;
  source->pitchable = pitchable;
  source->spatial = spatial;
  lovrRetain(source->sound);

  ma_data_converter_config config = ma_data_converter_config_init_default();
  config.formatIn = miniaudioFormats[lovrSoundGetFormat(sound)];
  config.formatOut = miniaudioFormats[OUTPUT_FORMAT];
  config.channelsIn = lovrSoundGetChannelCount(sound);
  config.channelsOut = lovrSoundGetChannelCount(sound);
  config.sampleRateIn = lovrSoundGetSampleRate(sound);
  config.sampleRateOut = state.config.sampleRate;
  config.allowDynamicSampleRate = pitchable;

  if (pitchable || config.formatIn != config.formatOut || config.sampleRateIn != config.sampleRateOut) {
    ma_data_converter* converter = lovrMalloc(sizeof(ma_data_converter));
    ma_result status = ma_data_converter_init(&config, NULL, converter);

    if (status != MA_SUCCESS) {
      lovrSetError("Problem creating Source data converter: %s (%d)", ma_result_description(status), status);
      lovrSourceDestroy(source);
      return NULL;
    }

    source->converter = converter;
  }

  if (!phonon_source_init(source)) {
    lovrSourceDestroy(source);
    return false;
  }

  return source;
}

Source* lovrSourceClone(Source* source) {
  Source* clone = lovrCalloc(sizeof(Source));
  clone->ref = 1;
  clone->slot = ~0u;
  clone->pitchRatio = source->pitchRatio;
  clone->volume = source->volume;
  vec3_init(clone->position, source->position);
  quat_init(clone->orientation, source->orientation);
  clone->radius = source->radius;
  vec3_init(clone->absorption, source->absorption);
  clone->innerAngle = source->innerAngle;
  clone->outerAngle = source->outerAngle;
  clone->outerAngleVolume = source->outerAngleVolume;
  clone->innerDistance = source->innerDistance;
  clone->minFalloffVolume = source->minFalloffVolume;
  clone->occlusionRays = source->occlusionRays;
  clone->transmissionRays = source->transmissionRays;
  clone->reverb = source->reverb;
  clone->reverbMode = source->reverbMode;
  clone->spatialization = source->spatialization;
  clone->looping = source->looping;
  clone->pitchable = source->pitchable;
  clone->spatial = source->spatial;

  if (source->converter) {
    ma_data_converter_config config = ma_data_converter_config_init_default();
    config.formatIn = source->converter->formatIn;
    config.formatOut = source->converter->formatOut;
    config.channelsIn = source->converter->channelsIn;
    config.channelsOut = source->converter->channelsOut;
    config.sampleRateIn = source->converter->sampleRateIn;
    config.sampleRateOut = source->converter->sampleRateOut;
    config.allowDynamicSampleRate = clone->pitchable;

    ma_data_converter* converter = lovrMalloc(sizeof(ma_data_converter));
    ma_result status = ma_data_converter_init(&config, NULL, converter);

    if (status != MA_SUCCESS) {
      lovrSetError("Problem creating Source data converter: %s (%d)", ma_result_description(status), status);
      lovrSourceDestroy(clone);
      return NULL;
    }

    clone->converter = converter;
  }

  clone->sound = source->sound;
  lovrRetain(clone->sound);

  if (!phonon_source_init(clone)) {
    lovrSourceDestroy(clone);
    return false;
  }

  return clone;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  phonon_source_destroy(source);
  lovrRelease(source->sound, lovrSoundDestroy);
  ma_data_converter_uninit(source->converter, NULL);
  lovrFree(source->converter);
  lovrFree(source);
}

Sound* lovrSourceGetSound(Source* source) {
  return source->sound;
}

bool lovrSourcePlay(Source* source) {
  if (state.config.autostart) {
    if (!state.devices[AUDIO_PLAYBACK].pContext) {
      lovrAudioSetDevice(AUDIO_PLAYBACK, NULL, 0, false, NULL, AUDIO_SHARED);
    }

    if (!lovrAudioIsStarted(AUDIO_PLAYBACK)) {
      lovrAudioStart(AUDIO_PLAYBACK);
    }

    state.config.autostart = false;
  }

  if (source->slot == ~0u) {
    uint64_t mask = state.activeSourceMask | state.pendingSourceMask;
    if (mask == ~0ull) return false;
    uint32_t slot = mask ? CTZL(~mask) : 0;
    state.pendingSourceMask |= (1ull << slot);
    state.activeSources[slot] = source;
    source->slot = slot;
    lovrRetain(source);
    phonon_source_add(source);
  }

  source->playRequest = 1;

  return true;
}

void lovrSourcePause(Source* source) {
  if (source->slot == ~0u) {
    source->playing = false;
  } else {
    source->playRequest = 0;
  }
}

void lovrSourceStop(Source* source) {
  lovrSourcePause(source);
  lovrSourceSeek(source, 0, UNIT_FRAMES);
}

bool lovrSourceIsPlaying(Source* source) {
  return source->playing || source->playRequest == 1;
}

bool lovrSourceIsLooping(Source* source) {
  return source->looping;
}

bool lovrSourceSetLooping(Source* source, bool loop) {
  lovrCheck(!loop || !lovrSoundIsStream(source->sound), "Can't loop streams");
  source->looping = loop;
  return true;
}

float lovrSourceGetPitch(Source* source) {
  return source->pitchRatio * ((float) state.config.sampleRate / lovrSoundGetSampleRate(source->sound));
}

bool lovrSourceSetPitch(Source* source, float pitch) {
  lovrCheck(pitch > 0.f, "Source pitch must be positive");
  lovrCheck(source->pitchable, "Source must be created with the 'pitch' flag to change its pitch");
  source->pitchRatio = pitch * ((float) lovrSoundGetSampleRate(source->sound) / state.config.sampleRate);
  return true;
}

float lovrSourceGetVolume(Source* source, VolumeUnit units) {
  return units == UNIT_LINEAR ? source->volume : linearToDb(source->volume);
}

void lovrSourceSetVolume(Source* source, float volume, VolumeUnit units) {
  if (units == UNIT_DECIBELS) volume = dbToLinear(volume);
  source->volume = CLAMP(volume, 0.f, 1.f);
}

void lovrSourceSeek(Source* source, double time, TimeUnit units) {
  source->seekRequest = units == UNIT_SECONDS ? (uint32_t) (time * lovrSoundGetSampleRate(source->sound) + .5) : (uint32_t) time;
}

double lovrSourceTell(Source* source, TimeUnit units) {
  return units == UNIT_SECONDS ? (double) source->offset / lovrSoundGetSampleRate(source->sound) : source->offset;
}

double lovrSourceGetDuration(Source* source, TimeUnit units) {
  uint32_t frames = lovrSoundGetFrameCount(source->sound);
  return units == UNIT_SECONDS ? (double) frames / lovrSoundGetSampleRate(source->sound) : frames;
}

void lovrSourceGetPose(Source* source, float position[3], float orientation[4]) {
  vec3_init(position, source->position);
  quat_init(orientation, source->orientation);
}

void lovrSourceSetPose(Source* source, float position[3], float orientation[4]) {
  if (position) vec3_init(source->position, position);
  if (orientation) quat_init(source->orientation, orientation);
  state.simulatorDirty = true;
}

float lovrSourceGetRadius(Source* source) {
  return source->radius;
}

void lovrSourceSetRadius(Source* source, float radius) {
  source->radius = radius;
  state.simulatorDirty = true;
}

bool lovrSourceIsSpatial(Source* source) {
  return source->spatial;
}

void lovrSourceGetAbsorption(Source* source, float absorption[3]) {
  vec3_init(absorption, source->absorption);
}

void lovrSourceSetAbsorption(Source* source, float absorption[3]) {
  vec3_init(source->absorption, absorption);
}

void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerVolume) {
  *innerAngle = source->innerAngle;
  *outerAngle = source->outerAngle;
  *outerVolume = source->outerAngleVolume;
}

void lovrSourceSetCone(Source* source, float innerAngle, float outerAngle, float outerVolume) {
  source->innerAngle = innerAngle;
  source->outerAngle = outerAngle;
  source->outerAngleVolume = outerVolume;
  state.simulatorDirty = true;
}

void lovrSourceGetFalloff(Source* source, float* innerDistance, float* minVolume) {
  *innerDistance = source->innerDistance;
  *minVolume = source->minFalloffVolume;
}

void lovrSourceSetFalloff(Source* source, float innerDistance, float minVolume) {
  source->innerDistance = innerDistance;
  source->minFalloffVolume = minVolume;
  state.simulatorDirty = true;
}

void lovrSourceGetOcclusion(Source* source, uint32_t* occlusionRays, uint32_t* transmissionRays) {
  *occlusionRays = source->occlusionRays;
  *transmissionRays = source->transmissionRays;
}

void lovrSourceSetOcclusion(Source* source, uint32_t occlusionRays, uint32_t transmissionRays) {
  source->occlusionRays = occlusionRays;
  source->transmissionRays = transmissionRays;
  state.simulatorDirty = true;
}

void lovrSourceGetReverb(Source* source, float* reverb, ReverbMode* mode) {
  *reverb = source->reverb;
  *mode = source->reverbMode;
}

void lovrSourceSetReverb(Source* source, float reverb, ReverbMode mode) {
  source->reverb = reverb;
  source->reverbMode = mode;
  state.simulatorDirty = true;
}

float lovrSourceGetSpatialization(Source* source) {
  return source->spatialization;
}

void lovrSourceSetSpatialization(Source* source, float spatialization) {
  source->spatialization = spatialization;
  state.simulatorDirty = true;
}

// AudioMesh

AudioMesh* lovrAudioMeshCreate(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial* materials, AudioMaterial material) {
  AudioMesh* mesh = lovrCalloc(sizeof(AudioMesh));
  mesh->ref = 1;
  mesh->enabled = true;
  mat4_identity(mesh->transform);

  if (!phonon_mesh_init(mesh, vertices, indices, vertexCount, indexCount, materials, material)) {
    lovrFree(mesh);
    return NULL;
  }

  return mesh;
}

AudioMesh* lovrAudioMeshClone(AudioMesh* parent) {
  AudioMesh* mesh = lovrCalloc(sizeof(AudioMesh));
  mesh->ref = 1;
  mesh->enabled = true;
  mesh->parent = parent;
  mat4_init(mesh->transform, parent->transform);

  if (!phonon_mesh_init_clone(mesh)) {
    lovrFree(mesh);
    return NULL;
  }

  lovrRetain(parent);
  return mesh;
}

void lovrAudioMeshDestroy(void* ref) {
  AudioMesh* mesh = ref;
  phonon_mesh_destroy(mesh);
  lovrRelease(mesh->parent, lovrAudioMeshDestroy);
  lovrFree(mesh);
}

bool lovrAudioMeshIsEnabled(AudioMesh* mesh) {
  return mesh->enabled;
}

void lovrAudioMeshSetEnabled(AudioMesh* mesh, bool enable) {
  phonon_mesh_set_enabled(mesh, enable);
  mesh->enabled = enable;
}

void lovrAudioMeshGetTransform(AudioMesh* mesh, float* transform) {
  mat4_init(transform, mesh->transform);
}

void lovrAudioMeshSetTransform(AudioMesh* mesh, float* transform) {
  phonon_mesh_set_transform(mesh, transform);
  mat4_init(mesh->transform, transform);
}

// Phonon

#ifdef LOVR_USE_PHONON

static void convertPose(float* position, float* orientation, IPLCoordinateSpace3* basis) {
  float transform[16];
  mat4_fromQuat(transform, orientation);
  vec3_init(&basis->right.x, &transform[0]);
  vec3_init(&basis->up.x, &transform[4]);
  vec3_scale(vec3_init(&basis->ahead.x, &transform[8]), -1.f);
  vec3_init(&basis->origin.x, position);
}

static float applyAttenuation(IPLfloat32 distance, void* userdata) {
  Source* source = userdata;
  if (distance <= source->innerDistance) {
    return 1.f;
  } else {
    return MAX(1.f / (1.f + MAX(distance - source->innerDistance, 0.f)), source->minFalloffVolume);
  }
}

static float applyDirectivity(IPLVector3 sourceToListener, void* userdata) {
  Source* source = userdata;

  float forward[3];
  vec3_set(forward, 0.f, 0.f, -1.f);
  float angle = vec3_angle(&sourceToListener.x, forward);

  if (angle <= source->innerAngle) {
    return 1.f;
  } else if (angle >= source->outerAngle) {
    return source->outerAngleVolume;
  } else if (source->innerAngle < source->outerAngle) {
    float t = (angle - source->innerAngle) / (source->outerAngle - source->innerAngle);
    return 1.f - (1.f - source->outerAngleVolume) * t;
  } else {
    return angle < source->innerAngle ? 1.f : source->outerAngleVolume;
  }
}

static void onSpatializerLog(IPLLogLevel iplLevel, const char* message) {
  int level;
  switch (iplLevel) {
    case IPL_LOGLEVEL_INFO: level = LOG_INFO; break;
    case IPL_LOGLEVEL_WARNING: level = LOG_WARN; break;
    case IPL_LOGLEVEL_ERROR: level = LOG_ERROR; break;
    case IPL_LOGLEVEL_DEBUG: level = LOG_DEBUG; break;
  }
  lovrLog(level, "SA", message);
}

static void simulateReflections(void* arg) {
  iplSimulatorRunReflections(state.simulator);
  atomic_store(&state.reverbFinished, true);
}

static bool phonon_init(void) {
  IPLContextSettings contextSettings = {
    .version = STEAMAUDIO_VERSION,
    .logCallback = onSpatializerLog,
    .simdLevel = IPL_SIMDLEVEL_AVX512
  };

  if (iplContextCreate(&contextSettings, &state.phonon)) {
    return lovrSetError("Failed to create SteamAudio context");
  }

  state.audioSettings.samplingRate = state.config.sampleRate;
  state.audioSettings.frameSize = BUFFER_SIZE;

  IPLReflectionEffectSettings reflectionSettings = {
    .type = state.config.reverb.type == REVERB_CONVOLUTION ?
      IPL_REFLECTIONEFFECTTYPE_CONVOLUTION :
      IPL_REFLECTIONEFFECTTYPE_PARAMETRIC,
    .irSize = state.config.reverb.duration * state.config.sampleRate,
    .numChannels = state.config.reverb.type == REVERB_CONVOLUTION ? 4 : 1
  };

  IPLSimulationSettings simulationSettings = {
    .flags = IPL_SIMULATIONFLAGS_DIRECT | (state.config.reverb.type ? IPL_SIMULATIONFLAGS_REFLECTIONS : 0),
    .sceneType = IPL_SCENETYPE_DEFAULT,
    .reflectionType = reflectionSettings.type,
    .maxNumOcclusionSamples = MAX_OCCLUSION_SAMPLES,
    .maxNumRays = state.config.reverb.rays,
    .numDiffuseSamples = 1024,
    .maxDuration = state.config.reverb.duration,
    .maxOrder = 1,
    .maxNumSources = MAX_SOURCES,
    .numThreads = 8,
    .samplingRate = state.audioSettings.samplingRate,
    .frameSize = state.audioSettings.frameSize
  };

  if (iplSimulatorCreate(state.phonon, &simulationSettings, &state.simulator)) {
    return phonon_destroy(), lovrSetError("Failed to create SteamAudio simulator");
  }

  IPLSceneSettings sceneSettings = { .type = IPL_SCENETYPE_DEFAULT };
  if (iplSceneCreate(state.phonon, &sceneSettings, &state.scene)) {
    return phonon_destroy(), lovrSetError("Failed to create SteamAudio scene");
  }

  iplSimulatorSetScene(state.simulator, state.scene);

  if (state.config.reverb.type != REVERB_NONE) {
    IPLSourceSettings sourceSettings = {
      .flags = IPL_SIMULATIONFLAGS_REFLECTIONS
    };

    if (iplSourceCreate(state.simulator, &sourceSettings, &state.listener)) {
      return phonon_destroy(), lovrSetError("Failed to create listener source");
    }

    if (iplReflectionEffectCreate(state.phonon, &state.audioSettings, &reflectionSettings, &state.reflectionEffect)) {
      return phonon_destroy(), lovrSetError("Failed to create reflection effect");
    }

    if (state.config.reverb.type == REVERB_CONVOLUTION) {
      if (iplReflectionMixerCreate(state.phonon, &state.audioSettings, &reflectionSettings, &state.reflectionMixer)) {
        return phonon_destroy(), lovrSetError("Failed to create reverb mixer");
      }

      IPLAmbisonicsDecodeEffectSettings settings = {
        .speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO,
        .maxOrder = 1
      };

      if (iplAmbisonicsDecodeEffectCreate(state.phonon, &state.audioSettings, &settings, &state.ambisonicsDecodeEffect)) {
        return phonon_destroy(), lovrSetError("Failed to create ambisonics decode effect");
      }
    }

    if (iplAudioBufferAllocate(state.phonon, 1, BUFFER_SIZE, &state.listenerReverb)) {
      return phonon_destroy(), lovrSetError("Failed to create reverb buffer");
    }

    if (iplAudioBufferAllocate(state.phonon, 1, BUFFER_SIZE, &state.parametricReverb)) {
      return phonon_destroy(), lovrSetError("Failed to create reverb buffer");
    }
  }

  atomic_store(&state.reverbFinished, true);

  return true;
}

static void phonon_destroy(void) {
  while (!atomic_load(&state.reverbFinished)) {
    job_spin();
  }

  phonon_set_hrtf(NULL);
  IPLHRTF hrtf = state.hrtf[0];
  iplHRTFRelease(&hrtf);
  iplAudioBufferFree(state.phonon, &state.parametricReverb);
  iplAudioBufferFree(state.phonon, &state.listenerReverb);
  iplAmbisonicsDecodeEffectRelease(&state.ambisonicsDecodeEffect), state.ambisonicsDecodeEffect = NULL;
  iplReflectionMixerRelease(&state.reflectionMixer), state.reflectionMixer = NULL;
  iplReflectionEffectRelease(&state.reflectionEffect), state.reflectionEffect = NULL;
  iplSourceRelease(&state.listener), state.listener = NULL;
  iplSceneRelease(&state.scene), state.scene = NULL;
  iplSimulatorRelease(&state.simulator), state.simulator = NULL;
  iplContextRelease(&state.phonon), state.phonon = NULL;
}

static void phonon_update(float dt) {
  if (state.sceneDirty) {
    iplSceneCommit(state.scene);
    state.simulatorDirty = true;
    state.sceneDirty = false;
  }

  if (!state.simulatorDirty) {
    return;
  }

  state.simulatorDirty = false;

  // TODO maybe split into 2 simulators so we can have less latency on direct simulation commits
  if (state.reverbFinished) {
    iplSimulatorCommit(state.simulator);
  }

  uint32_t backbuffer = state.backbuffer;
  uint64_t mask = state.activeSourceMask | state.pendingSourceMask;

  IPLSimulationSharedInputs sharedInputs;
  convertPose(state.position, state.orientation, &state.listenerBasis[backbuffer]);
  sharedInputs.listener = state.listenerBasis[backbuffer];
  sharedInputs.numRays = state.config.reverb.rays;
  sharedInputs.numBounces = state.config.reverb.bounces;
  sharedInputs.duration = state.config.reverb.duration;
  sharedInputs.order = 1;
  sharedInputs.irradianceMinDistance = .01f;
  iplSimulatorSetSharedInputs(state.simulator, IPL_SIMULATIONFLAGS_DIRECT, &sharedInputs);

  Source* source;
  FOREACH_SOURCE(mask, source) {
    IPLSimulationInputs inputs;
    inputs.flags = IPL_SIMULATIONFLAGS_DIRECT;

    inputs.directFlags = 0;
    if (vec3_dot(source->absorption, source->absorption) > 1e-5) inputs.directFlags |= IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION;
    if (source->outerAngleVolume < 1.f) inputs.directFlags |= IPL_DIRECTSIMULATIONFLAGS_DIRECTIVITY;
    if (source->minFalloffVolume < 1.f) inputs.directFlags |= IPL_DIRECTSIMULATIONFLAGS_DISTANCEATTENUATION;
    if (state.enabledMeshCount > 0 && source->occlusionRays > 0) inputs.directFlags |= IPL_DIRECTSIMULATIONFLAGS_OCCLUSION;
    if (state.enabledMeshCount > 0 && source->occlusionRays > 0 && source->transmissionRays > 0) inputs.directFlags |= IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION;

    convertPose(source->position, source->orientation, &inputs.source);

    float inverseListenerOrientation[4];
    quat_conjugate(quat_init(inverseListenerOrientation, state.orientation));
    vec3_sub(vec3_init(&source->relativeDirection[backbuffer].x, source->position), state.position);
    quat_rotate(inverseListenerOrientation, &source->relativeDirection[backbuffer].x);

    inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_CALLBACK;
    inputs.distanceAttenuationModel.callback = applyAttenuation;
    inputs.distanceAttenuationModel.userData = source;
    inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
    vec3_init(inputs.airAbsorptionModel.coefficients, source->absorption);
    inputs.directivity.callback = applyDirectivity;
    inputs.directivity.userData = source;
    inputs.occlusionType = source->radius > 0.f && source->occlusionRays > 1 ? IPL_OCCLUSIONTYPE_VOLUMETRIC : IPL_OCCLUSIONTYPE_RAYCAST;
    inputs.occlusionRadius = source->radius;
    inputs.numOcclusionSamples = source->radius > 0.f ? source->occlusionRays : 1;
    inputs.numTransmissionRays = source->transmissionRays;

    iplSourceSetInputs(source->handle, IPL_SIMULATIONFLAGS_DIRECT, &inputs);
  }

  iplSimulatorRunDirect(state.simulator);

  FOREACH_SOURCE(mask, source) {
    IPLSimulationOutputs outputs;
    iplSourceGetOutputs(source->handle, IPL_SIMULATIONFLAGS_DIRECT, &outputs);
    source->directParams[backbuffer] = outputs.direct;

    // gr
    IPLDirectEffectFlags flags = 0;
    if (vec3_dot(source->absorption, source->absorption) > 1e-5) flags |= IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION;
    if (source->outerAngleVolume < 1.f) flags |= IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY;
    if (source->minFalloffVolume < 1.f) flags |= IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION;
    if (state.enabledMeshCount > 0 && source->occlusionRays > 0) flags |= IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION;
    if (state.enabledMeshCount > 0 && source->occlusionRays > 0 && source->transmissionRays > 0) flags |= IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION;
    source->directParams[backbuffer].flags = flags;
  }

  uint64_t sourceReverbMask = 0;
  uint64_t listenerReverbMask = 0;

  if (state.config.reverb.type != REVERB_NONE && state.enabledMeshCount > 0) {
    FOREACH_SOURCE(mask, source) {
      if (source->spatial && source->reverb > 0.f) {
        if (source->reverbMode == REVERB_SOURCE) {
          sourceReverbMask |= (1u << source->slot);
        } else {
          listenerReverbMask |= (1u << source->slot);
        }
      }
    }

    if (listenerReverbMask && !state.listenerReverbMask) {
      iplSourceAdd(state.listener, state.simulator);
    } else if (!listenerReverbMask && state.listenerReverbMask) {
      iplSourceRemove(state.listener, state.simulator);
    }

    if (state.reverbFinished && (sourceReverbMask || listenerReverbMask)) {
      if (state.reverbSimulationTimer <= 0.f) {
        state.reverbSimulationTimer = state.config.reverb.rate;
        atomic_store(&state.reverbFinished, false);

        if (listenerReverbMask) {
          IPLSimulationInputs inputs;
          inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
          inputs.source = sharedInputs.listener;
          vec3_set(inputs.reverbScale, 1.f, 1.f, 1.f);
          iplSourceSetInputs(state.listener, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);
        }

        Source* source;
        FOREACH_SOURCE(sourceReverbMask, source) {
          IPLSimulationInputs inputs;
          inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
          convertPose(source->position, source->orientation, &inputs.source);
          vec3_set(inputs.reverbScale, 1.f, 1.f, 1.f);
          iplSourceSetInputs(source->handle, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);
        }

        iplSimulatorSetSharedInputs(state.simulator, IPL_SIMULATIONFLAGS_REFLECTIONS, &sharedInputs);

        while (!job_start(simulateReflections, NULL)) {
          job_spin();
        }
      } else {
        state.reverbSimulationTimer -= dt;
      }
    }
  }

  state.sourceReverbMask = sourceReverbMask;
  state.listenerReverbMask = listenerReverbMask;
  atomic_fetch_xor_explicit(&state.backbuffer, 1, memory_order_release);
}

static bool phonon_set_hrtf(Blob* blob) {
  IPLHRTF hrtf = NULL;

  if (blob) {
    IPLHRTFSettings settings = {
      .type = IPL_HRTFTYPE_SOFA,
      .sofaData = blob->data,
      .sofaDataSize = blob->size,
      .volume = 1.f,
      .normType = IPL_HRTFNORMTYPE_NONE
    };

    if (iplHRTFCreate(state.phonon, &state.audioSettings, &settings, &hrtf)) {
      return lovrSetError("Failed to create HRTF");
    }
  }

  IPLHRTF old = atomic_exchange(&state.hrtf[1], hrtf);

  if (old != NO_HRTF) {
    iplHRTFRelease(&old);
  }

  return true;
}

static void phonon_mix_begin(void) {
  state.frontbuffer = !atomic_load_explicit(&state.backbuffer, memory_order_acquire);

  IPLHRTF newHRTF = atomic_exchange(&state.hrtf[1], NO_HRTF);

  if (newHRTF != NO_HRTF) {
    IPLHRTF old = state.hrtf[0];
    iplHRTFRelease(&old);
    state.hrtf[0] = newHRTF;
  }

  if (state.config.reverb.type != REVERB_NONE) {
    memset(state.listenerReverb.data[0], 0, BUFFER_SIZE * sizeof(float));
    memset(state.parametricReverb.data[0], 0, BUFFER_SIZE * sizeof(float));
  }
}

static bool phonon_mix_source_ambisonic(Source* source, float* src, float* dst) {
  float left[BUFFER_SIZE], right[BUFFER_SIZE];
  IPLAudioBuffer output = { 2, BUFFER_SIZE, (float*[2]) { left, right } };
  bool tail = false;

  // Tail
  if (!source->playing) {
    if (iplAmbisonicsDecodeEffectGetTailSize(source->ambisonicEffect) > 0) {
      tail |= !iplAmbisonicsDecodeEffectGetTail(source->ambisonicEffect, &output);
      iplAudioBufferInterleave(state.phonon, &output, dst);
    } else {
      memset(dst, 0, 2 * BUFFER_SIZE * sizeof(float));
    }

    if (state.config.reverb.type != REVERB_NONE && iplReflectionEffectGetTailSize(source->reflectionEffect) > 0) {
      if (state.config.reverb.type == REVERB_CONVOLUTION) {
        tail |= !iplReflectionEffectGetTail(source->reflectionEffect, &output, state.reflectionMixer);
      } else {
        output.numChannels = 1;
        tail |= !iplReflectionEffectGetTail(source->reflectionEffect, &output, NULL);
        iplAudioBufferMix(state.phonon, &output, &state.parametricReverb);
      }
    }

    return tail;
  }

  IPLAudioBuffer input = source->ambisonicBuffer;
  iplAudioBufferDeinterleave(state.phonon, src, &input);
  iplAudioBufferConvertAmbisonics(state.phonon, IPL_AMBISONICSTYPE_SN3D, IPL_AMBISONICSTYPE_N3D, &input, &input);

  // Reverb
  if (source->reverb > 0.f && state.config.reverb.type != REVERB_NONE && state.enabledMeshCount > 0) {
    IPLAudioBuffer reverbInput = { 1, BUFFER_SIZE, NULL };

    if (source->reverb == 1.f) {
      reverbInput.data = input.data;
    } else {
      reverbInput.data = &src;
      for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        src[i] *= source->reverb;
      }
    }

    if (source->reverbMode == REVERB_SOURCE) {
      IPLSimulationOutputs outputs = { 0 };
      iplSourceGetOutputs(source->handle, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);

      if (state.config.reverb.type == REVERB_CONVOLUTION) {
        tail |= !iplReflectionEffectApply(source->reflectionEffect, &outputs.reflections, &reverbInput, &reverbInput, state.reflectionMixer);
      } else {
        IPLAudioBuffer out = { 1, BUFFER_SIZE, (float*[1]) { dst } };
        tail |= !iplReflectionEffectApply(source->reflectionEffect, &outputs.reflections, &reverbInput, &out, NULL);
        iplAudioBufferMix(state.phonon, &out, &state.parametricReverb);
      }
    } else {
      iplAudioBufferMix(state.phonon, &reverbInput, &state.listenerReverb);
    }
  }

  // Direct
  if (source->directParams[state.frontbuffer].flags) {
    tail |= !iplDirectEffectApply(source->directEffect, &source->directParams[state.frontbuffer], &input, &input);
  }

  // Spatialization
  if (source->spatialization > 0.f) {
    IPLAmbisonicsDecodeEffectParams params = {
      .order = AMBISONIC_ORDER(input.numChannels),
      .hrtf = state.hrtf[0],
      .binaural = !!state.hrtf[0]
    };

    float orientation[4];
    quat_conjugate(quat_init(orientation, source->orientation));
    quat_mul(orientation, orientation, state.orientation);
    convertPose((float[3]) { 0.f, 0.f, 0.f }, orientation, &params.orientation);

    tail |= !iplAmbisonicsDecodeEffectApply(source->ambisonicEffect, &params, &input, &output);

    if (source->spatialization < 1.f) {
      float s = source->spatialization, t = 1.f - s;
      for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        left[i] = left[i] * s + input.data[0][i] * t;
        right[i] = right[i] * s + input.data[0][i] * t;
      }
    }

    iplAudioBufferInterleave(state.phonon, &output, dst);
  } else {
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      dst[2 * i + 0] = input.data[0][i];
      dst[2 * i + 1] = input.data[0][i];
    }
  }

  return tail;
}

static bool phonon_mix_source(Source* source, float* src, float* dst) {
  float left[BUFFER_SIZE], right[BUFFER_SIZE];
  uint32_t channels = lovrSoundGetChannelCount(source->sound);
  bool tail = false;

  if (channels > 2) {
    return phonon_mix_source_ambisonic(source, src, dst);
  }

  // Tail
  if (!source->playing) {
    IPLAudioBuffer buffer = { 2, BUFFER_SIZE, (float*[2]) { left, right } };

    if (iplBinauralEffectGetTailSize(source->binauralEffect) > 0) {
      tail |= !iplBinauralEffectGetTail(source->binauralEffect, &buffer);
      iplAudioBufferInterleave(state.phonon, &buffer, dst);
    } else {
      memset(dst, 0, 2 * BUFFER_SIZE * sizeof(float));
    }

    if (state.config.reverb.type != REVERB_NONE && iplReflectionEffectGetTailSize(source->reflectionEffect) > 0) {
      if (state.config.reverb.type == REVERB_CONVOLUTION) {
        tail |= !iplReflectionEffectGetTail(source->reflectionEffect, &buffer, state.reflectionMixer);
      } else {
        buffer.numChannels = 1;
        tail |= !iplReflectionEffectGetTail(source->reflectionEffect, &buffer, NULL);
        iplAudioBufferMix(state.phonon, &buffer, &state.parametricReverb);
      }
    }

    return tail;
  }

  // Prepare input (since we always copy src, it can be reused as a temporary buffer after this)
  IPLAudioBuffer input = { channels, BUFFER_SIZE, (float*[2]) { left, right } };

  if (channels == 1) {
    input.data[1] = left; // Alias both channels, useful for spatialization
    memcpy(left, src, BUFFER_SIZE * sizeof(float));
  } else {
    iplAudioBufferDeinterleave(state.phonon, src, &input);
  }

  // Reverb input
  if (source->reverb > 0.f && state.config.reverb.type != REVERB_NONE && state.enabledMeshCount > 0) {
    if (channels == 2) {
      for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        src[i] = (left[i] + right[i]) * .5f * source->reverb;
      }
    } else if (source->reverb != 1.f) {
      for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        src[i] *= source->reverb;
      }
    }

    IPLAudioBuffer reverbInput = { 1, BUFFER_SIZE, &src };

    if (source->reverbMode == REVERB_SOURCE) {
      IPLSimulationOutputs outputs = { 0 };
      iplSourceGetOutputs(source->handle, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);

      if (state.config.reverb.type == REVERB_CONVOLUTION) {
        tail |= !iplReflectionEffectApply(source->reflectionEffect, &outputs.reflections, &reverbInput, &reverbInput, state.reflectionMixer);
      } else {
        IPLAudioBuffer out = { 1, BUFFER_SIZE, (float*[1]) { dst } };
        tail |= !iplReflectionEffectApply(source->reflectionEffect, &outputs.reflections, &reverbInput, &out, NULL);
        iplAudioBufferMix(state.phonon, &out, &state.parametricReverb);
      }
    } else {
      iplAudioBufferMix(state.phonon, &reverbInput, &state.listenerReverb);
    }
  }

  // Direct effects, applied in-place
  if (source->directParams[state.frontbuffer].flags) {
    tail |= !iplDirectEffectApply(source->directEffect, &source->directParams[state.frontbuffer], &input, &input);
  }

  // Spatialization (either binaural, panning, or upmix)
  if (source->spatialization > 0.f) {
    // Can reuse src as temporary buffer for spatialization output
    IPLAudioBuffer spatialized = { 2, BUFFER_SIZE, (float*[2]) { src, src + BUFFER_SIZE } };

    if (state.hrtf[0]) {
      IPLBinauralEffectParams params = {
        .direction = source->relativeDirection[state.frontbuffer],
        .interpolation = IPL_HRTFINTERPOLATION_BILINEAR,
        .spatialBlend = source->spatialization,
        .hrtf = state.hrtf[0]
      };
      input.numChannels = 2; // For mono input, left/right channels are aliased to same buffer
      tail |= !iplBinauralEffectApply(source->binauralEffect, &params, &input, &spatialized);
    } else {
      IPLAudioBuffer mono = { 1, BUFFER_SIZE, .data = channels == 2 ? &dst : input.data };

      // Stereo input uses dst as temporary buffer to hold downmixed audio, for panning effect
      if (channels == 2) {
        for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
          dst[i] = (left[i] + right[i]) * .5f;
        }
      }

      IPLPanningEffectParams params = { .direction = source->relativeDirection[state.frontbuffer] };
      iplPanningEffectApply(source->panningEffect, &params, &mono, &spatialized);

      if (source->spatialization < 1.f) {
        float s = source->spatialization, t = 1.f - s;
        for (uint32_t c = 0; c < 2; c++) {
          for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
            spatialized.data[c][i] = spatialized.data[c][i] * s + input.data[c][i] * t;
          }
        }
      }
    }

    iplAudioBufferInterleave(state.phonon, &spatialized, dst);
  } else {
    input.numChannels = 2;
    iplAudioBufferInterleave(state.phonon, &input, dst);
  }

  return tail;
}

static void phonon_mix_reverb(float* dst) {
  float left[BUFFER_SIZE], right[BUFFER_SIZE];

  IPLAudioBuffer mono = { 1, BUFFER_SIZE, (float*[1]) { left } };
  IPLAudioBuffer stereo = { 2, BUFFER_SIZE, (float*[2]) { left, right } };

  bool anyReverb = state.sourceReverbMask || state.listenerReverbMask || state.reverbDecay > 0.f;

  // Listener-centric reverb
  if (state.listenerReverbMask || state.reverbDecay > 0.f) {
    IPLSimulationOutputs outputs = { 0 };
    iplSourceGetOutputs(state.listener, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);

    if (state.config.reverb.type == REVERB_CONVOLUTION) {
      iplReflectionEffectApply(state.reflectionEffect, &outputs.reflections, &state.listenerReverb, &mono, state.reflectionMixer);
    } else {
      iplReflectionEffectApply(state.reflectionEffect, &outputs.reflections, &state.listenerReverb, &mono, NULL);
      iplAudioBufferMix(state.phonon, &mono, &state.parametricReverb);
    }

    // After reverbing sources finish playing, we keep feeding the effect silence for a bit, so that
    // we can reset it once it decays, to avoid any cut off tails or pops if we have to reverb again
    if (state.listenerReverbMask) {
      state.reverbDecay = state.config.reverb.duration * BUFFER_SIZE;
    }
  } else if (iplReflectionEffectGetTailSize(state.reflectionEffect) > 0) {
    bool complete = false;

    if (state.config.reverb.type == REVERB_CONVOLUTION) {
      complete = iplReflectionEffectGetTail(state.reflectionEffect, &mono, state.reflectionMixer);
    } else {
      complete = iplReflectionEffectGetTail(state.reflectionEffect, &mono, NULL);
      iplAudioBufferMix(state.phonon, &mono, &state.parametricReverb);
    }

    if (complete) {
      iplReflectionEffectReset(state.reflectionEffect);
    }

    anyReverb = true;
  }

  // Final reverb mix
  if (state.config.reverb.type == REVERB_CONVOLUTION) {
    if (anyReverb) {
      float a[BUFFER_SIZE], b[BUFFER_SIZE], c[BUFFER_SIZE], d[BUFFER_SIZE];
      IPLAudioBuffer ambisonic = { 4, BUFFER_SIZE, (float*[4]) { a, b, c, d } };
      IPLReflectionEffectParams reflectionMixerParams = { .numChannels = 4 };
      iplReflectionMixerApply(state.reflectionMixer, &reflectionMixerParams, &ambisonic);

      IPLAmbisonicsDecodeEffectParams ambisonicsDecodeParams = {
        .order = 1,
        .hrtf = state.hrtf[0],
        .orientation = state.listenerBasis[state.frontbuffer],
        .binaural = !!state.hrtf[0]
      };

      iplAmbisonicsDecodeEffectApply(state.ambisonicsDecodeEffect, &ambisonicsDecodeParams, &ambisonic, &stereo);
    } else if (iplAmbisonicsDecodeEffectGetTailSize(state.ambisonicsDecodeEffect) > 0) {
      iplAmbisonicsDecodeEffectGetTail(state.ambisonicsDecodeEffect, &stereo);
    } else {
      return;
    }

    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      dst[2 * i + 0] += stereo.data[0][i];
      dst[2 * i + 1] += stereo.data[1][i];
    }
  } else if (anyReverb) {
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      dst[2 * i + 0] += state.parametricReverb.data[0][i];
      dst[2 * i + 1] += state.parametricReverb.data[0][i];
    }
  }
}

static bool phonon_source_init(Source* source) {
  IPLSourceSettings settings = {
    .flags = IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS
  };

  if (iplSourceCreate(state.simulator, &settings, &source->handle)) {
    return lovrSetError("Failed to add source to spatializer");
  }

  IPLDirectEffectSettings directEffectSettings = {
    .numChannels = lovrSoundGetChannelCount(source->sound)
  };

  IPLPanningEffectSettings panningEffectSettings = {
    .speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO
  };

  IPLBinauralEffectSettings binauralEffectSettings = {
    .hrtf = state.hrtf[0]
  };

  IPLReflectionEffectSettings reflectionSettings = {
    .type = state.config.reverb.type == REVERB_CONVOLUTION ?
      IPL_REFLECTIONEFFECTTYPE_CONVOLUTION :
      IPL_REFLECTIONEFFECTTYPE_PARAMETRIC,
    .irSize = state.config.reverb.duration * state.config.sampleRate,
    .numChannels = state.config.reverb.type == REVERB_CONVOLUTION ? 4 : 1
  };

  if (iplDirectEffectCreate(state.phonon, &state.audioSettings, &directEffectSettings, &source->directEffect)) {
    lovrSetError("Failed to create direct effect");
    phonon_source_destroy(source);
    return false;
  }

  if (iplPanningEffectCreate(state.phonon, &state.audioSettings, &panningEffectSettings, &source->panningEffect)) {
    lovrSetError("Failed to create panning effect");
    phonon_source_destroy(source);
    return false;
  }

  if (iplReflectionEffectCreate(state.phonon, &state.audioSettings, &reflectionSettings, &source->reflectionEffect)) {
    lovrSetError("Failed to create reflection effect");
    phonon_source_destroy(source);
    return false;
  }

  if (lovrSoundGetChannelCount(source->sound) > 2) {
    uint32_t channels = lovrSoundGetChannelCount(source->sound);

    IPLAmbisonicsDecodeEffectSettings ambisonicSettings = {
      .speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO,
      .maxOrder = AMBISONIC_ORDER(channels)
    };

    if (iplAmbisonicsDecodeEffectCreate(state.phonon, &state.audioSettings, &ambisonicSettings, &source->ambisonicEffect)) {
      lovrSetError("Failed to create ambisonic effect");
      phonon_source_destroy(source);
      return false;
    }

    if (iplAudioBufferAllocate(state.phonon, channels, BUFFER_SIZE, &source->ambisonicBuffer)) {
      lovrSetError("Failed to create audio buffer");
      phonon_source_destroy(source);
      return false;
    }
  } else {
    if (iplBinauralEffectCreate(state.phonon, &state.audioSettings, &binauralEffectSettings, &source->binauralEffect)) {
      lovrSetError("Failed to create binaural effect");
      phonon_source_destroy(source);
      return false;
    }
  }

  return true;
}

static void phonon_source_destroy(Source* source) {
  iplAudioBufferFree(state.phonon, &source->ambisonicBuffer), source->ambisonicBuffer.data = NULL;
  iplAmbisonicsDecodeEffectRelease(&source->ambisonicEffect), source->ambisonicEffect = NULL;
  iplReflectionEffectRelease(&source->reflectionEffect), source->reflectionEffect = NULL;
  iplBinauralEffectRelease(&source->binauralEffect), source->binauralEffect = NULL;
  iplPanningEffectRelease(&source->panningEffect), source->panningEffect = NULL;
  iplDirectEffectRelease(&source->directEffect), source->directEffect = NULL;
  iplSourceRelease(&source->handle), source->handle = NULL;
}

static void phonon_source_reset(Source* source) {
  iplDirectEffectReset(source->directEffect);
  iplPanningEffectReset(source->panningEffect);
  iplBinauralEffectReset(source->binauralEffect);
  iplReflectionEffectReset(source->reflectionEffect);
  iplAmbisonicsDecodeEffectReset(source->ambisonicEffect);
}

static void phonon_source_add(Source* source) {
  iplSourceAdd(source->handle, state.simulator);
  state.simulatorDirty = true;
}

static void phonon_source_remove(Source* source) {
  iplSourceRemove(source->handle, state.simulator);
  state.simulatorDirty = true;
}

static IPLMaterial materialData[] = {
  [MATERIAL_GENERIC] = { { .10f, .20f, .30f }, .05f, { .100f, .050f, .030f } },
  [MATERIAL_BRICK] = { { .03f, .04f, .07f }, .05f, { .015f, .015f, .015f } },
  [MATERIAL_CARPET] = { { .24f, .69f, .73f }, .05f, { .020f, .005f, .003f } },
  [MATERIAL_CERAMIC] = { { .01f, .02f, .02f }, .05f, { .060f, .044f, .011f } },
  [MATERIAL_CONCRETE] = { { .05f, .07f, .08f }, .05f, { .015f, .002f, .001f } },
  [MATERIAL_GLASS] = { { .06f, .03f, .02f }, .05f, { .060f, .044f, .011f } },
  [MATERIAL_GRAVEL] = { { .60f, .70f, .80f }, .05f, { .031f, .012f, .008f } },
  [MATERIAL_METAL] = { { .20f, .07f, .06f }, .05f, { .200f, .25f, .010f } },
  [MATERIAL_PLASTER] = { { .12f, .06f, .04f }, .05f, { .056f, .056f, .004f } },
  [MATERIAL_ROCK] = { { .13f, .20f, .24f }, .05f, { .015f, .002f, .001f } },
  [MATERIAL_WOOD] = { { .11f, .07f, .06f }, .05f, { .070f, .014f, .005f } }
};

static bool phonon_mesh_init(AudioMesh* mesh, float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial* materials, AudioMaterial material) {
  lovrCheck(indexCount % 3 == 0, "AudioMesh index count must be a multiple of 3");

  // Scene

  IPLSceneSettings sceneSettings = {
    .type = IPL_SCENETYPE_DEFAULT
  };

  if (iplSceneCreate(state.phonon, &sceneSettings, &mesh->scene)) {
    lovrSetError("Failed to create AudioMesh scene");
    return NULL;
  }

  // StaticMesh

  IPLStaticMeshSettings settings = {
    .numVertices = vertexCount,
    .numTriangles = indexCount / 3,
    .numMaterials = COUNTOF(materialData),
    .vertices = (IPLVector3*) vertices,
    .materials = materialData
  };

  settings.triangles = lovrMalloc(settings.numTriangles * sizeof(IPLTriangle));

  for (int i = 0; i < settings.numTriangles; i++) {
    settings.triangles[i].indices[0] = indices[3 * i + 0];
    settings.triangles[i].indices[1] = indices[3 * i + 1];
    settings.triangles[i].indices[2] = indices[3 * i + 2];
  }

  if (materials) {
    settings.materialIndices = (IPLint32*) materials;
  } else {
    settings.materialIndices = lovrMalloc(settings.numTriangles * sizeof(IPLint32));
    for (int i = 0; i < settings.numTriangles; i++) {
      settings.materialIndices[i] = material;
    }
  }

  bool success = !iplStaticMeshCreate(mesh->scene, &settings, &mesh->staticMesh);
  if (!materials) lovrFree(settings.materialIndices);
  lovrFree(settings.triangles);

  if (!success) {
    lovrSetError("Failed to create AudioMesh");
    iplSceneRelease(&mesh->scene);
    return false;
  }

  iplStaticMeshAdd(mesh->staticMesh, mesh->scene);
  iplSceneCommit(mesh->scene);

  // InstancedMesh

  IPLInstancedMeshSettings instancedMeshSettings = {
    .subScene = mesh->scene,
    .transform.elements = {
      { 1.f, 0.f, 0.f, 0.f },
      { 0.f, 1.f, 0.f, 0.f },
      { 0.f, 0.f, 1.f, 0.f },
      { 0.f, 0.f, 0.f, 1.f }
    }
  };

  if (iplInstancedMeshCreate(state.scene, &instancedMeshSettings, &mesh->instancedMesh)) {
    lovrSetError("Failed to add AudioMesh to scene");
    iplStaticMeshRelease(&mesh->staticMesh);
    iplSceneRelease(&mesh->scene);
    return false;
  }

  iplInstancedMeshAdd(mesh->instancedMesh, state.scene);
  state.enabledMeshCount++;
  state.sceneDirty = true;
  return true;
}

static bool phonon_mesh_init_clone(AudioMesh* mesh) {
  IPLInstancedMeshSettings settings = {
    .subScene = mesh->parent->scene
  };

  mat4_transpose(mat4_init(&settings.transform.elements[0][0], mesh->transform));

  if (iplInstancedMeshCreate(state.scene, &settings, &mesh->instancedMesh)) {
    lovrSetError("Failed to create instanced audio mesh");
    return false;
  }

  iplInstancedMeshAdd(mesh->instancedMesh, state.scene);

  mesh->staticMesh = mesh->parent->staticMesh;
  mesh->scene = mesh->parent->scene;
  state.enabledMeshCount++;
  state.sceneDirty = true;
  return true;
}

static void phonon_mesh_destroy(AudioMesh* mesh) {
  if (mesh->enabled) {
    iplInstancedMeshRemove(mesh->instancedMesh, state.scene);
    state.enabledMeshCount--;
    state.sceneDirty = true;
  }
  iplInstancedMeshRelease(&mesh->instancedMesh);
  iplStaticMeshRelease(&mesh->staticMesh);
  iplSceneRelease(&mesh->scene);
}

static void phonon_mesh_set_enabled(AudioMesh* mesh, bool enable) {
  if (mesh->enabled != enable) {
    if (enable) {
      iplInstancedMeshAdd(mesh->instancedMesh, state.scene);
      state.enabledMeshCount++;
    } else {
      iplInstancedMeshRemove(mesh->instancedMesh, state.scene);
      state.enabledMeshCount--;
    }
    state.sceneDirty = true;
  }
}

static void phonon_mesh_set_transform(AudioMesh* mesh, float* transform) {
  IPLMatrix4x4 matrix;
  mat4_transpose(mat4_init(&matrix.elements[0][0], transform));
  iplInstancedMeshUpdateTransform(mesh->instancedMesh, state.scene, matrix);
  state.sceneDirty = true;
}

#else
static bool phonon_init(void) { return true; }
static void phonon_destroy(void) {}
static void phonon_update(float dt) {}
static bool phonon_set_hrtf(Blob* blob) { return true; }
static void phonon_mix_begin(void) {}
static bool phonon_mix_source(Source* source, float* src, float* dst) { return false; }
static void phonon_mix_reverb(float* dst) {}
static bool phonon_source_init(Source* source) { return true; }
static void phonon_source_destroy(Source* source) {}
static void phonon_source_reset(Source* source) {}
static void phonon_source_add(Source* source) {}
static void phonon_source_remove(Source* source) {}
static bool phonon_mesh_init(AudioMesh* mesh, float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial* materials, AudioMaterial material) { return true; }
static bool phonon_mesh_init_clone(AudioMesh* clone) { return true; }
static void phonon_mesh_destroy(AudioMesh* mesh) {}
static void phonon_mesh_set_enabled(AudioMesh* mesh, bool enable) {}
static void phonon_mesh_set_transform(AudioMesh* mesh, float* transform) {}
#endif
