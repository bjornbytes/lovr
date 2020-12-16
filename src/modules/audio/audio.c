#include "audio/audio.h"
#include "data/soundData.h"
#include "data/blob.h"
#include "core/arr.h"
#include "core/ref.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib/miniaudio/miniaudio.h"
#include "audio/spatializer.h"

static const ma_format miniAudioFormatFromLovr[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

#define OUTPUT_FORMAT SAMPLE_F32
#define OUTPUT_CHANNELS 2
#define CAPTURE_CHANNELS 1

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
  float transform[16];
};

static inline int outputChannelCountForSource(Source *source) { return source->spatial ? 1 : OUTPUT_CHANNELS; }

static struct {
  bool initialized;
  ma_context context;
  AudioConfig config[AUDIO_TYPE_COUNT];
  ma_device devices[AUDIO_TYPE_COUNT];
  ma_mutex playbackLock;
  Source* sources;
  SoundData *captureStream;
  arr_t(ma_data_converter*) converters;
  Spatializer* spatializer;

  AudioDevice *deviceInfos;
} state;

// Device callbacks

static bool mix(Source* source, float* output, uint32_t count) {
  float raw[2048];
  float aux[2048];
  float mix[4096];

  // TODO
  // frameLimitIn =
  // frameLimitOut =

  while (count > 0) {
    uint32_t chunk = MIN(sizeof(raw) / SampleFormatBytesPerFrame(source->sound->channels, source->sound->format),
        ma_data_converter_get_required_input_frame_count(source->converter, count));
        // ^^^ Note need to min `count` with 'capacity of aux buffer' and 'capacity of mix buffer'
        // could skip min-ing with one of the buffers if you can guarantee that one is bigger/equal to the other (you can because their formats are known)
    ma_uint64 framesIn = source->sound->read(source->sound, source->offset, chunk, raw);
    ma_uint64 framesOut = sizeof(aux) / (sizeof(float) * outputChannelCountForSource(source));

    ma_data_converter_process_pcm_frames(source->converter, raw, &framesIn, aux, &framesOut);

    if (source->spatial) {
      state.spatializer->apply(source, source->transform, aux, mix, framesOut);
    } else {
      memcpy(mix, aux, framesOut * SampleFormatBytesPerFrame(OUTPUT_CHANNELS, SAMPLE_F32));
    }

    for (uint32_t i = 0; i < framesOut * OUTPUT_CHANNELS; i++) {
      output[i] += mix[i] * source->volume;
    }

    if (framesIn == 0) {
      source->offset = 0;
      if (!source->looping) {
        source->playing = false;
        return false;
      }
    } else {
      source->offset += framesIn;
    }

    count -= framesOut;
    output += framesOut * OUTPUT_CHANNELS;
  }

  return true;
}

static void onPlayback(ma_device* device, void* output, const void* _, uint32_t count) {
  ma_mutex_lock(&state.playbackLock);

  // For each Source, remove it if it isn't playing or process it and remove it if it stops
  for (Source** list = &state.sources, *source = *list; source != NULL; source = *list) {
    if (source->playing && mix(source, output, count)) {
      list = &source->next;
    } else {
      *list = source->next;
      source->tracked = false;
      lovrRelease(Source, source);
    }
  }

  ma_mutex_unlock(&state.playbackLock);
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t frames) {
  // note: uses ma_pcm_rb which is lockless
  size_t bytesPerFrame = SampleFormatBytesPerFrame(CAPTURE_CHANNELS, OUTPUT_FORMAT);
  lovrSoundDataStreamAppendBuffer(state.captureStream, input, frames*bytesPerFrame);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer *spatializers[] = {
  &dummySpatializer,
};

// Entry

bool lovrAudioInit(AudioConfig config[2]) {
  if (state.initialized) return false;

  memcpy(state.config, config, sizeof(state.config));

  if (ma_context_init(NULL, 0, NULL, &state.context)) {
    return false;
  }

  int mutexStatus = ma_mutex_init(&state.playbackLock);
  lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");

  lovrAudioReset();

  for (int i = 0; i < AUDIO_TYPE_COUNT; i++) {
    if (config[i].enable && config[i].start) {
      int startStatus = ma_device_start(&state.devices[i]);
      if(startStatus != MA_SUCCESS) {
        lovrAudioDestroy();
        lovrAssert(false, "Failed to start audio device %d\n", i);
        return false;
      }
    }
  }

  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (spatializers[i]->init()) {
      state.spatializer = spatializers[i];
      break;
    }
  }
  lovrAssert(state.spatializer != NULL, "Must have at least one spatializer");

  arr_init(&state.converters);

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.devices[AUDIO_PLAYBACK]);
  ma_device_uninit(&state.devices[AUDIO_CAPTURE]);
  ma_mutex_uninit(&state.playbackLock);
  ma_context_uninit(&state.context);
  lovrRelease(SoundData, state.captureStream);
  if (state.spatializer) state.spatializer->destroy();
  for(int i = 0; i < state.converters.length; i++) {
    ma_data_converter_uninit(state.converters.data[i]);
    free(state.converters.data[i]);
  }
  arr_free(&state.converters);
  memset(&state, 0, sizeof(state));
}

bool lovrAudioInitDevice(AudioType type) {
  ma_device_type deviceType = (type == AUDIO_PLAYBACK) ? ma_device_type_playback : ma_device_type_capture;

  ma_device_config config = ma_device_config_init(deviceType);
  config.sampleRate = state.config[type].sampleRate;
  lovrAssert(state.config[AUDIO_PLAYBACK].format == OUTPUT_FORMAT, "Only f32 playback format currently supported");
  config.playback.format = miniAudioFormatFromLovr[state.config[AUDIO_PLAYBACK].format];
  config.capture.format = miniAudioFormatFromLovr[state.config[AUDIO_CAPTURE].format];
  config.playback.pDeviceID = state.config[AUDIO_PLAYBACK].device;
  config.capture.pDeviceID = state.config[AUDIO_CAPTURE].device;
  config.playback.channels = OUTPUT_CHANNELS;
  config.capture.channels = CAPTURE_CHANNELS;
  config.dataCallback = callbacks[type];
  config.performanceProfile = ma_performance_profile_low_latency;

  int err = ma_device_init(&state.context, &config, &state.devices[type]); 
  if (err != MA_SUCCESS) {
    lovrLog(LOG_WARN, "audio", "Failed to enable audio device %d: %d\n", type, err);
    return false;
  }

  if (type == AUDIO_CAPTURE) {
    lovrRelease(SoundData, state.captureStream);
    state.captureStream = lovrSoundDataCreateStream(state.config[type].sampleRate * 1.0, CAPTURE_CHANNELS, state.config[type].sampleRate, state.config[type].format);
    if (!state.captureStream) {
      lovrLog(LOG_WARN, "audio", "Failed to init audio device %d\n", type);
      lovrAudioDestroy();
      return false;
    }
  }

  return true;
}

bool lovrAudioReset() {
  for (int i = 0; i < AUDIO_TYPE_COUNT; i++) {
    // clean up previous state ...
    if (state.devices[i].state != 0) {
      ma_device_uninit(&state.devices[i]);
    }

    // .. and create new one
    if (state.config[i].enable) {
      lovrAudioInitDevice(i);
    }
  }

  return true;
}

bool lovrAudioStart(AudioType type) {
  if (state.config[type].enable == false) {
    if (lovrAudioInitDevice(type) == false) {
      if (type == AUDIO_CAPTURE) {
        lovrPlatformRequestPermission(AUDIO_CAPTURE_PERMISSION);
        // lovrAudioStart will be retried from boot.lua upon permission granted event
      }
      return false;
    }
    state.config[type].enable = state.config[type].start = true;
  }
  int status = ma_device_start(&state.devices[type]);
  return status == MA_SUCCESS;
}

bool lovrAudioStop(AudioType type) {
  return ma_device_stop(&state.devices[type]) == MA_SUCCESS;
}

float lovrAudioGetVolume() {
  float volume = 0.f;
  ma_device_get_master_volume(&state.devices[AUDIO_PLAYBACK], &volume);
  return volume;
}

void lovrAudioSetVolume(float volume) {
  ma_device_set_master_volume(&state.devices[AUDIO_PLAYBACK], volume);
}

void lovrAudioSetListenerPose(float position[4], float orientation[4]) {
  state.spatializer->setListenerPose(position, orientation);
}

double lovrAudioConvertToSeconds(uint32_t sampleCount, AudioType context) {
  return sampleCount / (double)state.config[context].sampleRate;
}

// Source

static void _lovrSourceAssignConverter(Source *source) {
  source->converter = NULL;
  for (size_t i = 0; i < state.converters.length; i++) {
    ma_data_converter* converter = state.converters.data[i];
    if (converter->config.formatIn != miniAudioFormatFromLovr[source->sound->format]) continue;
    if (converter->config.sampleRateIn != source->sound->sampleRate) continue;
    if (converter->config.channelsIn != source->sound->channels) continue;
    if (converter->config.channelsOut != outputChannelCountForSource(source)) continue;
    source->converter = converter;
    break;
  }

  if (!source->converter) {
    ma_data_converter_config config = ma_data_converter_config_init_default();
    config.formatIn = miniAudioFormatFromLovr[source->sound->format];
    config.formatOut = miniAudioFormatFromLovr[OUTPUT_FORMAT];
    config.channelsIn = source->sound->channels;
    config.channelsOut = outputChannelCountForSource(source);
    config.sampleRateIn = source->sound->sampleRate;
    config.sampleRateOut = state.config[AUDIO_PLAYBACK].sampleRate;

    ma_data_converter *converter = malloc(sizeof(ma_data_converter));
    ma_result converterStatus = ma_data_converter_init(&config, converter);
    lovrAssert(converterStatus == MA_SUCCESS, "Problem creating Source data converter #%d: %d", state.converters.length, converterStatus);
    
    arr_expand(&state.converters, 1);
    state.converters.data[state.converters.length++] = source->converter = converter;
  }
}

Source* lovrSourceCreate(SoundData* sound, bool spatial) {
  Source* source = lovrAlloc(Source);
  source->sound = sound;
  lovrRetain(source->sound);
  source->volume = 1.f;
  
  source->spatial = spatial;
  mat4_identity(source->transform);
  _lovrSourceAssignConverter(source);

  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrRelease(SoundData, source->sound);
}

void lovrSourcePlay(Source* source) {
  ma_mutex_lock(&state.playbackLock);

  source->playing = true;

  if (!source->tracked) {
    lovrRetain(source);
    source->tracked = true;
    source->next = state.sources;
    state.sources = source;
  }

  ma_mutex_unlock(&state.playbackLock);
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
  lovrAssert(loop == false || lovrSoundDataIsStream(source->sound) == false, "Can't loop streams");
  source->looping = loop;
}

float lovrSourceGetVolume(Source* source) {
  return source->volume;
}

void lovrSourceSetVolume(Source* source, float volume) {
  ma_mutex_lock(&state.playbackLock);
  source->volume = volume;
  ma_mutex_unlock(&state.playbackLock);
}

bool lovrSourceGetSpatial(Source *source) {
  return source->spatial;
}

void lovrSourceSetPose(Source *source, float position[4], float orientation[4]) {
  ma_mutex_lock(&state.playbackLock);
  mat4_identity(source->transform);
  mat4_translate(source->transform, position[0], position[1], position[2]);
  mat4_rotate(source->transform, orientation[0], orientation[1], orientation[2], orientation[3]);
  ma_mutex_unlock(&state.playbackLock);
}

uint32_t lovrSourceGetTime(Source* source) {
  if (lovrSoundDataIsStream(source->sound)) {
    return 0;
  } else {
    return source->offset;
  }
}

void lovrSourceSetTime(Source* source, uint32_t time) {
  ma_mutex_lock(&state.playbackLock);
  source->offset = time;
  ma_mutex_unlock(&state.playbackLock);
}

SoundData* lovrSourceGetSoundData(Source* source) {
  return source->sound;
}

// Capture

struct SoundData* lovrAudioGetCaptureStream()
{
  return state.captureStream;
}

// Devices

void lovrAudioGetDevices(AudioDevice **outDevices, size_t *outCount) {
  if(state.deviceInfos) 
    free(state.deviceInfos);
  
  ma_result gettingStatus = ma_context_get_devices(&state.context, NULL, NULL, NULL, NULL);
  lovrAssert(gettingStatus == MA_SUCCESS, "Failed to enumerate audio devices: %d", gettingStatus);
  *outCount = state.context.playbackDeviceInfoCount + state.context.captureDeviceInfoCount;
  *outDevices = state.deviceInfos = calloc(*outCount, sizeof(AudioDevice));
  for(int i = 0; i < *outCount; i++) {
    ma_device_info *mainfo = &state.context.pDeviceInfos[i];
    AudioDevice *lovrInfo = &state.deviceInfos[i];
    lovrInfo->name = mainfo->name;
    lovrInfo->type = i < state.context.playbackDeviceInfoCount ? AUDIO_PLAYBACK : AUDIO_CAPTURE;
    lovrInfo->isDefault = mainfo->isDefault; // remove _private after bumping miniaudio
    lovrInfo->identifier = &mainfo->id;
    lovrInfo->minChannels = mainfo->minChannels;
    lovrInfo->maxChannels = mainfo->maxChannels;
  }
}

void lovrAudioUseDevice(AudioDeviceIdentifier identifier, int sampleRate, SampleFormat format) {
  int deviceCount = state.context.playbackDeviceInfoCount + state.context.captureDeviceInfoCount;
  for(int i = 0; i < deviceCount; i++) {
    if (identifier == &state.context.pDeviceInfos[i].id) {
      AudioType type = i < state.context.playbackDeviceInfoCount ? AUDIO_PLAYBACK : AUDIO_CAPTURE;
      state.config[type].device = identifier;
      if (sampleRate) state.config[type].sampleRate = sampleRate;
      if (format != SAMPLE_INVALID) state.config[type].format = format;
      lovrLog(LOG_INFO, "audio", "Switching to %s device %s (%p)", type?"capture":"playback", state.context.pDeviceInfos[i].name, identifier);
      ma_device_uninit(&state.devices[type]);
      if(state.config[type].enable)
        lovrAudioInitDevice(type);
      if(state.config[type].start)
        ma_device_start(&state.devices[type]);

      return;
    }
  }
  lovrAssert(false, "Couldn't find the given identifier");
}