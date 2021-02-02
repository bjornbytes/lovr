#include "audio/audio.h"
#include "audio/spatializer.h"
#include "data/soundData.h"
#include "data/blob.h"
#include "core/arr.h"
#include "core/ref.h"
#include "core/os.h"
#include "core/util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

#define OUTPUT_FORMAT SAMPLE_F32
#define OUTPUT_CHANNELS 2
#define CAPTURE_CHANNELS 1
#define CALLBACK_PERIODS 3
#define PERIOD_LENGTH 128
#define CALLBACK_LENGTH (PERIOD_LENGTH*CALLBACK_PERIODS)

//#define LOVR_DEBUG_AUDIOTAP
#ifdef LOVR_DEBUG_AUDIOTAP
// To get a record of what the audio callback is playing, define LOVR_DEBUG_AUDIOTAP,
// after running look in the lovr save directory for lovrDebugAudio.raw,
// and open as raw 32-bit stereo floats (Audacity can do this, or Amadeus on Mac)
#include "filesystem/filesystem.h"
#endif

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

typedef struct {
  char *deviceName;
  uint32_t sampleRate;
  SampleFormat format;
} AudioConfig;

static uint32_t outputChannelCountForSource(Source *source) { return source->spatial ? 1 : OUTPUT_CHANNELS; }

static struct {
  bool initialized;
  ma_context context;
  AudioConfig config[2];
  ma_device devices[2];
  ma_mutex lock;
  Source* sources;
  SoundData *captureStream;
  arr_t(ma_data_converter*) converters;
  float position[4];
  float orientation[4];
  Spatializer* spatializer;
  bool fixedBuffer;
  uint32_t bufferSize;
  float *scratchBuffer1, *scratchBuffer2; // Used internally by mix(). Contains bufferSize stereo frames.
  float *persistBuffer;  // If fixedBuffer, preserves excess audio between frames.
  float *persistBufferContent; // Pointer into persistBuffer
  uint32_t persistBufferRemaining; // In fixedBuffer mode, how much of the previous frame's mixBuffer was consumed?
#ifdef LOVR_DEBUG_AUDIOTAP
  bool audiotapWriting;
#endif
} state;

// Device callbacks

// Return value is number of stereo frames in buffer
// Note: output is always equal to scratchBuffer1. This saves a little memory but is ugly.
//       count is always less than or equal to the size of scratchBuffer1
static int generateSource(Source* source, float* output, uint32_t count) {
  // Scratch buffers: Raw generated audio from source; converted to float by converter
  char* raw;
  float* aux;

  if (source->spatial) { // In spatial mode, raw and aux are mono and only output is stereo
    raw = (char*) state.scratchBuffer1;
    aux = state.scratchBuffer2;
  } else { // Otherwise, the data converter will produce stereo and aux=output is stereo
    raw = (char*) state.scratchBuffer2;
    aux = output;
  }

  bool sourceFinished = false;
  ma_uint64 framesIn = 0;
  while (framesIn < count) { // Read from source until raw buffer filled
    ma_uint64 framesRequested = count - framesIn;
    // FIXME: Buffer size math will break (crash) if channels > 2
    ma_uint64 framesRead = source->sound->read(source->sound, source->offset, framesRequested,
      raw + framesIn * SampleFormatBytesPerFrame(source->sound->channels, source->sound->format));
    framesIn += framesRead;
    if (framesRead < framesRequested) {
      source->offset = 0;
      if (!source->looping) { // Source has reached its final end
        sourceFinished = true;
        break;
      }
    } else {
      source->offset += framesRead;
    }
  }
  // 1 channel for spatial, 2 otherwise
  ma_uint64 framesConverted = framesIn * outputChannelCountForSource(source);

  // We assume framesConverted is not changed by calling this and discard its value
  ma_data_converter_process_pcm_frames(source->converter, raw, &framesIn, aux, &framesConverted);

  ma_uint64 framesOut = framesIn;

  if (source->spatial) {
    // Fixed buffer mode we have to pad buffer with silence if it underran
    if (state.fixedBuffer && sourceFinished) {
      memset(aux + framesIn, 0, (count - framesIn) * sizeof(float)); // Note always mono
      framesOut = count;
    }
    framesOut = state.spatializer->apply(source, aux, output, framesIn, framesOut);
  }

  if (sourceFinished) {
    lovrSourcePause(source);
  }

  return framesOut;
}

static void onPlayback(ma_device* device, void* outputUntyped, const void* _, uint32_t count) {
  float* output = outputUntyped;

#ifdef LOVR_DEBUG_AUDIOTAP
  int originalCount = count;
#endif

  // This case means we are in fixedBuffer mode and there was excess data generated last frame
  if (state.persistBufferRemaining > 0) {
    uint32_t persistConsumed = MIN(count, state.persistBufferRemaining);
    memcpy(output, state.persistBufferContent, persistConsumed * OUTPUT_CHANNELS * sizeof(float)); // Stereo frames
    // Move forward both the persistBufferContent and output pointers so the right thing happens regardless of which is larger
    // persistBufferRemaining being larger than count is deeply unlikely, but it is not impossible
    state.persistBufferContent += persistConsumed*OUTPUT_CHANNELS;
    state.persistBufferRemaining -= persistConsumed;
    output += persistConsumed * OUTPUT_CHANNELS;
    count -= persistConsumed;
  }

  while (count > 0) { // Mixing will be done in a series of passes
    ma_mutex_lock(&state.lock);

    // Usually we mix directly into the output buffer.
    // But if we're in fixed buffer mode and the fixed buffer size is bigger than output,
    // we mix into persistBuffer and save the excess until next onPlayback() call.
    uint32_t passSize;
    float* mixBuffer;
    bool usingPersistBuffer;
    if (state.fixedBuffer) {
      usingPersistBuffer = state.bufferSize > count;
      passSize = state.bufferSize;
      if (usingPersistBuffer) {
        mixBuffer = state.persistBuffer;
        state.persistBufferRemaining = state.bufferSize;
        memset(mixBuffer, 0, state.bufferSize * sizeof(float) * OUTPUT_CHANNELS);
      } else {
        mixBuffer = output;
        state.persistBufferRemaining = 0;
      }
    } else {
      usingPersistBuffer = false;
      mixBuffer = output;
      passSize = MIN(count, state.bufferSize); // In non-fixedBuffer mode we can use a buffer smaller than bufferSize, but not larger
    }

    // For each Source, remove it if it isn't playing or process it and remove it if it stops
    for (Source** list = &state.sources, *source = *list; source != NULL; source = *list) {
      bool playing = source->playing;
      if (playing) {
        // Generate audio
        uint32_t generated = generateSource(source, state.scratchBuffer1, passSize);
        playing = source->playing; // Can change during generateSource

        // Mix and apply volume
        for (uint32_t i = 0; i < generated * OUTPUT_CHANNELS; i++) {
          mixBuffer[i] += state.scratchBuffer1[i] * source->volume;
        }
      }

      // Iterate/manage list
      if (playing) {
        list = &source->next;
      } else {
        *list = source->next;
        source->tracked = false;
        lovrRelease(Source, source);
      }
    }
    {
      uint32_t tailGenerated = state.spatializer->tail(state.scratchBuffer2, state.scratchBuffer1, passSize);
      // Mix tail
      for (uint32_t i = 0; i < tailGenerated * OUTPUT_CHANNELS; i++) {
        mixBuffer[i] += state.scratchBuffer1[i];
      }
    }
    ma_mutex_unlock(&state.lock);

    if (usingPersistBuffer) { // Copy persist buffer into output (if needed)
      // Remember, in this scenario state.persistBuffer is mixBuffer and we just overwrote it in full
      memcpy(output, state.persistBuffer, count * OUTPUT_CHANNELS * sizeof(float));
      state.persistBufferContent = state.persistBuffer + count * OUTPUT_CHANNELS;
      state.persistBufferRemaining -= count;
      count = 0;
    } else {
      output += passSize * OUTPUT_CHANNELS;
      count -= passSize;
    }
  }

#ifdef LOVR_DEBUG_AUDIOTAP
  if (state.audiotapWriting) {
    lovrFilesystemWrite("lovrDebugAudio.raw", outputUntyped, originalCount * OUTPUT_CHANNELS * sizeof(float), true);
  }
#endif
}

static void onCapture(ma_device* device, void* output, const void* input, uint32_t frames) {
  size_t bytesPerFrame = SampleFormatBytesPerFrame(CAPTURE_CHANNELS, state.config[AUDIO_CAPTURE].format);
  lovrSoundDataStreamAppendBuffer(state.captureStream, (float*) input, frames * bytesPerFrame);
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer* spatializers[] = {
#ifdef LOVR_ENABLE_OCULUS_AUDIO
  &oculusSpatializer,
#endif
  &dummySpatializer
};

// Entry

bool lovrAudioInit(SpatializerConfig config) {
  if (state.initialized) return false;

  state.config[AUDIO_PLAYBACK] = (AudioConfig) { .format = SAMPLE_F32, .sampleRate = 44100 };
  state.config[AUDIO_CAPTURE] = (AudioConfig) { .format = SAMPLE_F32, .sampleRate = 44100 };

  if (ma_context_init(NULL, 0, NULL, &state.context)) {
    return false;
  }

  int mutexStatus = ma_mutex_init(&state.lock);
  lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");

  SpatializerConfigIn spatializerConfigIn = {
    .maxSourcesHint = config.spatializerMaxSourcesHint,
    .fixedBuffer = CALLBACK_LENGTH,
    .sampleRate = state.config[AUDIO_PLAYBACK].sampleRate
  };

  SpatializerConfigOut spatializerConfigOut = { 0 };

  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (config.spatializer && strcmp(config.spatializer, spatializers[i]->name)) {
      continue;
    }

    if (spatializers[i]->init(spatializerConfigIn, &spatializerConfigOut)) {
      state.spatializer = spatializers[i];
      break;
    }
  }
  lovrAssert(state.spatializer, "Must have at least one spatializer");

  state.fixedBuffer = spatializerConfigOut.needFixedBuffer;
  state.bufferSize = state.fixedBuffer ? CALLBACK_LENGTH : 1024;
  state.scratchBuffer1 = malloc(state.bufferSize * sizeof(float) * OUTPUT_CHANNELS);
  state.scratchBuffer2 = malloc(state.bufferSize * sizeof(float) * OUTPUT_CHANNELS);
  if (state.fixedBuffer) {
    state.persistBuffer = malloc(state.bufferSize * sizeof(float) * OUTPUT_CHANNELS);
  }
  state.persistBufferRemaining = 0;

  arr_init(&state.converters);

#ifdef LOVR_DEBUG_AUDIOTAP
  lovrFilesystemWrite("lovrDebugAudio.raw", NULL, 0, false); // Erase file
  state.audiotapWriting = true;
#endif

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
  free(state.config[0].deviceName);
  free(state.config[1].deviceName);
  free(state.scratchBuffer1);
  free(state.scratchBuffer2);
  free(state.persistBuffer);
  memset(&state, 0, sizeof(state));
}

bool lovrAudioInitDevice(AudioType type) {
  ma_device_info* playbackDevices;
  ma_uint32 playbackDeviceCount;
  ma_device_info* captureDevices;
  ma_uint32 captureDeviceCount;
  ma_result gettingStatus = ma_context_get_devices(&state.context, &playbackDevices, &playbackDeviceCount, &captureDevices, &captureDeviceCount);
  lovrAssert(gettingStatus == MA_SUCCESS, "Failed to enumerate audio devices during initialization: %s (%d)", ma_result_description(gettingStatus), gettingStatus);

  ma_device_config config;
  if (type == AUDIO_PLAYBACK) {
    ma_device_type deviceType = ma_device_type_playback;
    config = ma_device_config_init(deviceType);

    lovrAssert(state.config[AUDIO_PLAYBACK].format == OUTPUT_FORMAT, "Only f32 playback format currently supported");
    config.playback.format = miniaudioFormats[state.config[AUDIO_PLAYBACK].format];
    for (uint32_t i = 0; i < playbackDeviceCount && state.config[AUDIO_PLAYBACK].deviceName; i++) {
      if (strcmp(playbackDevices[i].name, state.config[AUDIO_PLAYBACK].deviceName) == 0) {
        config.playback.pDeviceID = &playbackDevices[i].id;
      }
    }

    if (state.config[AUDIO_PLAYBACK].deviceName && config.playback.pDeviceID == NULL) {
      lovrLog(LOG_WARN, "audio", "No audio playback device called '%s'; falling back to default.", state.config[AUDIO_PLAYBACK].deviceName);
    }

    config.playback.channels = OUTPUT_CHANNELS;
  } else {
    ma_device_type deviceType = ma_device_type_capture;
    config = ma_device_config_init(deviceType);

    config.capture.format = miniaudioFormats[state.config[AUDIO_CAPTURE].format];
    for (uint32_t i = 0; i < captureDeviceCount && state.config[AUDIO_CAPTURE].deviceName; i++) {
      if (strcmp(captureDevices[i].name, state.config[AUDIO_CAPTURE].deviceName) == 0) {
        config.capture.pDeviceID = &captureDevices[i].id;
      }
    }

    if (state.config[AUDIO_CAPTURE].deviceName && config.capture.pDeviceID == NULL) {
      lovrLog(LOG_WARN, "audio", "No audio capture device called '%s'; falling back to default.", state.config[AUDIO_CAPTURE].deviceName);
    }

    config.capture.channels = CAPTURE_CHANNELS;
  }

  config.periodSizeInFrames = PERIOD_LENGTH;
  config.periods = 3;
  config.performanceProfile = ma_performance_profile_low_latency;
  config.dataCallback = callbacks[type];
  config.sampleRate = state.config[type].sampleRate;

  ma_result err = ma_device_init(&state.context, &config, &state.devices[type]);
  if (err != MA_SUCCESS) {
    lovrLog(LOG_WARN, "audio", "Failed to enable %s audio device: %s (%d)\n", type == AUDIO_PLAYBACK ? "playback" : "capture", ma_result_description(err), err);
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

bool lovrAudioStart(AudioType type) {
  ma_uint32 deviceState = state.devices[type].state;
  if (deviceState == MA_STATE_UNINITIALIZED) {
    if (!lovrAudioInitDevice(type)) {
      if (type == AUDIO_CAPTURE) {
        lovrPlatformRequestPermission(AUDIO_CAPTURE_PERMISSION);
        // by default, lovrAudioStart will be retried from boot.lua upon permission granted event
      }
      return false;
    }
  }
  return ma_device_start(&state.devices[type]) == MA_SUCCESS;
}

bool lovrAudioStop(AudioType type) {
  return ma_device_stop(&state.devices[type]) == MA_SUCCESS;
}

bool lovrAudioIsStarted(AudioType type) {
  return ma_device_get_state(&state.devices[type]) == MA_STATE_STARTED;
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

AudioDeviceArr* lovrAudioGetDevices(AudioType type) {
  ma_device_info* playbackDevices;
  ma_uint32 playbackDeviceCount;
  ma_device_info* captureDevices;
  ma_uint32 captureDeviceCount;
  ma_result gettingStatus = ma_context_get_devices(&state.context, &playbackDevices, &playbackDeviceCount, &captureDevices, &captureDeviceCount);
  lovrAssert(gettingStatus == MA_SUCCESS, "Failed to enumerate audio devices: %s (%d)", ma_result_description(gettingStatus), gettingStatus);

  ma_uint32 count = type == AUDIO_PLAYBACK ? playbackDeviceCount : captureDeviceCount;
  ma_device_info* madevices = type == AUDIO_PLAYBACK ? playbackDevices : captureDevices;
  AudioDeviceArr* devices = calloc(1, sizeof(AudioDeviceArr));
  devices->capacity = devices->length = count;
  devices->data = calloc(count, sizeof(AudioDevice));

  for (uint32_t i = 0; i < count; i++) {
    ma_device_info* mainfo = &madevices[i];
    AudioDevice* lovrInfo = &devices->data[i];
    lovrInfo->name = strdup(mainfo->name);
    lovrInfo->type = type;
    lovrInfo->isDefault = mainfo->isDefault;
  }

  return devices;
}

void lovrAudioFreeDevices(AudioDeviceArr *devices) {
  for (size_t i = 0; i < devices->length; i++) {
    free((void*) devices->data[i].name);
  }
  arr_free(devices);
}

void lovrAudioSetCaptureFormat(SampleFormat format, uint32_t sampleRate) {
  if (sampleRate) state.config[AUDIO_CAPTURE].sampleRate = sampleRate;
  if (format != SAMPLE_INVALID) state.config[AUDIO_CAPTURE].format = format;

  // restart device if needed
  ma_uint32 previousState = state.devices[AUDIO_CAPTURE].state;
  if (previousState != MA_STATE_UNINITIALIZED) {
    ma_device_uninit(&state.devices[AUDIO_CAPTURE]);
    if (previousState == MA_STATE_STARTED) {
      lovrAudioStart(AUDIO_CAPTURE);
    }
  }
}

void lovrAudioUseDevice(AudioType type, const char* deviceName) {
  free(state.config[type].deviceName);

#ifdef ANDROID
  // XX<nevyn> miniaudio doesn't seem to be happy to set a specific device an android (fails with
  // error -2 on device init). Since there is only one playback and one capture device in OpenSL,
  // we can just set this to NULL and make this call a no-op.
  deviceName = NULL;
#endif

  state.config[type].deviceName = deviceName ? strdup(deviceName) : NULL;

  // restart device if needed
  ma_uint32 previousState = state.devices[type].state;
  if (previousState != MA_STATE_UNINITIALIZED) {
    ma_device_uninit(&state.devices[type]);
    if (previousState == MA_STATE_STARTED) {
      lovrAudioStart(type);
    }
  }
}

const char* lovrAudioGetSpatializer() {
  return state.spatializer->name;
}

// Source

Source* lovrSourceCreate(SoundData* sound, bool spatial) {
  Source* source = lovrAlloc(Source);
  source->sound = sound;
  lovrRetain(source->sound);

  source->volume = 1.f;
  source->spatial = spatial;

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
    config.sampleRateOut = state.config[AUDIO_PLAYBACK].sampleRate;

    ma_data_converter* converter = malloc(sizeof(ma_data_converter));
    ma_result converterStatus = ma_data_converter_init(&config, converter);
    lovrAssert(converterStatus == MA_SUCCESS, "Problem creating Source data converter #%d: %s (%d)", state.converters.length, ma_result_description(converterStatus), converterStatus);

    arr_push(&state.converters, converter);
    source->converter = converter;
  }

  state.spatializer->sourceCreate(source);

  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  state.spatializer->sourceDestroy(source);
  lovrRelease(SoundData, source->sound);
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

void lovrSourceStop(Source* source) {
  lovrSourcePause(source);
  lovrSourceSetTime(source, 0, UNIT_SAMPLES);
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
  uint32_t frames = lovrSoundDataGetDuration(source->sound);
  return units == UNIT_SECONDS ? (double) frames / source->sound->sampleRate : frames;
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
