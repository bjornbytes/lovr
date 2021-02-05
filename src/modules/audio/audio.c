#include "audio/audio.h"
#include "audio/spatializer.h"
#include "data/soundData.h"
#include "core/arr.h"
#include "core/ref.h"
#include "core/os.h"
#include "core/util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

#define OUTPUT_FORMAT SAMPLE_F32
#define OUTPUT_CHANNELS 2
#define CAPTURE_CHANNELS 1
#define MAX_SOURCES 64
#define BUFFER_SIZE 256

struct Source {
  Source* next;
  SoundData* sound;
  ma_data_converter* converter;
  intptr_t spatializerMemo; // Spatializer can put anything it wants here
  uint32_t offset;
  float volume;
  float position[4];
  float orientation[4];
  bool tracked;
  bool playing;
  bool looping;
  bool spatial;
};

static uint32_t outputChannelCountForSource(Source *source) { return source->spatial ? 1 : OUTPUT_CHANNELS; }

static struct {
  bool initialized;
  ma_context context;
  ma_device devices[2];
  ma_mutex lock;
  Source* sources;
  SoundData* captureStream;
  arr_t(ma_data_converter*) converters;
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

    for (Source** list = &state.sources, *source = *list; source != NULL; source = *list) {
      if (!source->playing) {
        *list = source->next;
        source->tracked = false;
        lovrRelease(Source, source);
        continue;
      }

      // Read and convert raw frames until there's BUFFER_SIZE converted frames
      uint32_t channels = outputChannelCountForSource(source);
      uint64_t frameLimit = sizeof(raw) / source->sound->channels / sizeof(float);
      uint32_t framesToConvert = BUFFER_SIZE;
      uint32_t framesConverted = 0;
      while (framesToConvert > 0) {
        src = raw;
        uint64_t framesToRead = source->converter ? MIN(ma_data_converter_get_required_input_frame_count(source->converter, framesToConvert), frameLimit) : framesToConvert;
        uint64_t framesRead = source->sound->read(source->sound, source->offset, framesToRead, src);
        ma_uint64 framesIn = framesRead;
        ma_uint64 framesOut = framesToConvert;

        if (source->converter) {
          ma_data_converter_process_pcm_frames(source->converter, src, &framesIn, aux + framesConverted * channels, &framesOut);
          src = aux;
        }

        if (framesRead < framesToRead) {
          source->offset = 0;
          if (!source->looping) {
            source->playing = false;
            memset(src + framesConverted * channels, 0, framesToConvert * channels * sizeof(float));
            break;
          }
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

      list = &source->next;
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

static void onCapture(ma_device* device, void* output, const void* input, uint32_t frames) {
  size_t bytesPerFrame = SampleFormatBytesPerFrame(CAPTURE_CHANNELS, state.captureStream->format);
  lovrSoundDataStreamAppendBuffer(state.captureStream, (float*) input, frames * bytesPerFrame);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer* spatializers[] = {
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
    .fixedBuffer = BUFFER_SIZE,
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

  arr_init(&state.converters);
  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < 2; i++) {
    ma_device_uninit(&state.devices[i]);
  }
  ma_mutex_uninit(&state.lock);
  ma_context_uninit(&state.context);
  lovrRelease(SoundData, state.captureStream);
  if (state.spatializer) state.spatializer->destroy();
  for (size_t i = 0; i < state.converters.length; i++) {
    ma_data_converter_uninit(state.converters.data[i]);
    free(state.converters.data[i]);
  }
  arr_free(&state.converters);
  memset(&state, 0, sizeof(state));
}

const char* lovrAudioGetSpatializer() {
  return state.spatializer->name;
}

static LOVR_THREAD_LOCAL struct {
  AudioType type;
  AudioDeviceCallback* callback;
} enumerateContext;

static ma_bool32 enumerateCallback(ma_context* context, ma_device_type type, const ma_device_info* info, void* userdata) {
  if (type == (enumerateContext.type == AUDIO_PLAYBACK ? ma_device_type_playback : ma_device_type_capture)) {
    AudioDevice device = {
      .id = &info->id,
      .idSize = sizeof(info->id),
      .name = info->name,
      .isDefault = info->isDefault
    };

    enumerateContext.callback(&device, userdata);
  }

  return MA_TRUE;
}

void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata) {
  enumerateContext.type = type;
  enumerateContext.callback = callback;
  ma_context_enumerate_devices(&state.context, enumerateCallback, userdata);
}

bool lovrAudioSetDevice(AudioType type, void* id, size_t size, uint32_t sampleRate, SampleFormat format) {
  if (id && size != sizeof(ma_device_id)) return false;

#ifdef ANDROID
  // XX<nevyn> miniaudio doesn't seem to be happy to set a specific device an android (fails with
  // error -2 on device init). Since there is only one playback and one capture device in OpenSL,
  // we can just set this to NULL and make this call a no-op.
  id = NULL;
#endif

  ma_device_config config;

  if (type == AUDIO_PLAYBACK) {
    lovrAssert(sampleRate == PLAYBACK_SAMPLE_RATE, "Playback sample rate must be %d", PLAYBACK_SAMPLE_RATE);
    lovrAssert(format == SAMPLE_F32, "Playback format must be f32");
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = (ma_device_id*) id;
    config.playback.format = miniaudioFormats[format];
    config.playback.channels = OUTPUT_CHANNELS;
  } else {
    config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = (ma_device_id*) id;
    config.capture.format = miniaudioFormats[format];
    config.capture.channels = CAPTURE_CHANNELS;
    lovrRelease(SoundData, state.captureStream);
    state.captureStream = lovrSoundDataCreateStream(sampleRate * 1., CAPTURE_CHANNELS, sampleRate, format);
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

struct SoundData* lovrAudioGetCaptureStream() {
  return state.captureStream;
}

// Source

Source* lovrSourceCreate(SoundData* sound, bool spatial) {
  Source* source = lovrAlloc(Source);
  source->sound = sound;
  lovrRetain(source->sound);

  source->volume = 1.f;
  source->spatial = spatial;

  if (sound->format != OUTPUT_FORMAT || sound->channels != outputChannelCountForSource(source) || sound->sampleRate != PLAYBACK_SAMPLE_RATE) {
    for (size_t i = 0; i < state.converters.length; i++) {
      ma_data_converter* converter = state.converters.data[i];
      if (converter->config.formatIn != miniaudioFormats[sound->format]) continue;
      if (converter->config.sampleRateIn != sound->sampleRate) continue;
      if (converter->config.channelsIn != sound->channels) continue;
      if (converter->config.channelsOut != outputChannelCountForSource(source)) continue;
      source->converter = converter;
      break;
    }

    if (!source->converter) {
      ma_data_converter_config config = ma_data_converter_config_init_default();
      config.formatIn = miniaudioFormats[sound->format];
      config.formatOut = miniaudioFormats[OUTPUT_FORMAT];
      config.channelsIn = sound->channels;
      config.channelsOut = outputChannelCountForSource(source);
      config.sampleRateIn = sound->sampleRate;
      config.sampleRateOut = PLAYBACK_SAMPLE_RATE;

      ma_data_converter* converter = malloc(sizeof(ma_data_converter));
      ma_result converterStatus = ma_data_converter_init(&config, converter);
      lovrAssert(converterStatus == MA_SUCCESS, "Problem creating Source data converter #%d: %s (%d)", state.converters.length, ma_result_description(converterStatus), converterStatus);

      arr_push(&state.converters, converter);
      source->converter = converter;
    }
  }

  state.spatializer->sourceCreate(source);

  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  state.spatializer->sourceDestroy(source);
  lovrRelease(SoundData, source->sound);
}

bool lovrSourcePlay(Source* source) {
  ma_mutex_lock(&state.lock);

  source->playing = true;

  if (!source->tracked) {
    lovrRetain(source);
    source->tracked = true;
    source->next = state.sources;
    state.sources = source;
  }

  ma_mutex_unlock(&state.lock);
  return true;
}

void lovrSourcePause(Source* source) {
  source->playing = false;
}

void lovrSourceStop(Source* source) {
  lovrSourcePause(source);
  lovrSourceSetTime(source, 0, UNIT_FRAMES);
}

bool lovrSourceIsPlaying(Source* source) {
  return source->playing;
}

bool lovrSourceIsLooping(Source* source) {
  return source->looping;
}

void lovrSourceSetLooping(Source* source, bool loop) {
  lovrAssert(loop == false || lovrSoundDataIsStream(source->sound) == false, "Can't loop streams");
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

double lovrSourceGetDuration(Source* source, TimeUnit units) {
  return units == UNIT_SECONDS ? (double) source->sound->frames / source->sound->sampleRate : source->sound->frames;
}

double lovrSourceGetTime(Source* source, TimeUnit units) {
  return units == UNIT_SECONDS ? (double) source->offset / source->sound->sampleRate : source->offset;
}

void lovrSourceSetTime(Source* source, double time, TimeUnit units) {
  ma_mutex_lock(&state.lock);
  source->offset = units == UNIT_SECONDS ? (uint32_t) (time * source->sound->sampleRate + .5) : (uint32_t) time;
  ma_mutex_unlock(&state.lock);
}

intptr_t* lovrSourceGetSpatializerMemoField(Source* source) {
  return &source->spatializerMemo;
}
