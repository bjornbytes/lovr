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
  uint32_t offset;
  float volume;
  bool tracked;
  bool playing;
  bool looping;
  bool spatial;
  float position[4];
  float orientation[4];
  intptr_t spatializerMemo; // Spatializer can put anything it wants here
};

static inline int outputChannelCountForSource(Source *source) { return source->spatial ? 1 : OUTPUT_CHANNELS; }

static struct {
  bool initialized;
  ma_context context;
  AudioDeviceConfig deviceConfig[AUDIO_TYPE_COUNT];
  ma_device devices[AUDIO_TYPE_COUNT];
  ma_mutex playbackLock;
  Source* sources;
  ma_pcm_rb captureRingbuffer;
  arr_t(ma_data_converter*) converters;
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
  char *raw; float *aux; // Scratch buffers: Raw generated audio from source; converted to float by converter
  if (source->spatial) { // In spatial mode, raw and aux are mono and only output is stereo
    raw = (char *)state.scratchBuffer1;
    aux = state.scratchBuffer2;
  } else {               // Otherwise, the data converter will produce stereo and aux=output is stereo
    raw = (char *)state.scratchBuffer2;
    aux = output;
  }

  bool sourceFinished = false;
  ma_uint64 framesIn = 0;
  while (framesIn < count) { // Read from source until raw buffer filled
    ma_uint64 framesRequested = count - framesIn;
    // FIXME: Buffer size math will break (crash) if channels > 1
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
      memset(aux + framesIn, 0, (count - framesIn)*sizeof(float)); // Note always mono
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
  float *output = outputUntyped;

#ifdef LOVR_DEBUG_AUDIOTAP
  int originalCount = count;
#endif

//  printf("Persist buffer remaining %d, Count %d\n", state.persistBufferRemaining, count); // Can't call lovrLog from audio thread

  // This case means we are in fixedBuffer mode and there was excess data generated last frame
  if (state.persistBufferRemaining > 0) {
    uint32_t persistConsumed = MIN(count, state.persistBufferRemaining);
    memcpy(output, state.persistBufferContent, persistConsumed*OUTPUT_CHANNELS*sizeof(float)); // Stereo frames
    // Move forward both the persistBufferContent and output pointers so the right thing happens regardless of which is larger
    // persistBufferRemaining being larger than count is deeply unlikely, but it is not impossible
    state.persistBufferContent += persistConsumed*OUTPUT_CHANNELS;
    state.persistBufferRemaining -= persistConsumed;
    output += persistConsumed*OUTPUT_CHANNELS;
    count -= persistConsumed;
  }

  while (count > 0) { // Mixing will be done in a series of passes
    ma_mutex_lock(&state.playbackLock);

    // Usually we mix directly into the output buffer.
    // But if we're in fixed buffer mode and the fixed buffer size is bigger than output,
    // we mix into persistBuffer and save the excess until next onPlayback() call.
    uint32_t passSize;
    float *mixBuffer;
    bool usingPersistBuffer;
    if (state.fixedBuffer) {
      usingPersistBuffer = state.bufferSize > count;
      passSize = state.bufferSize;
      if (usingPersistBuffer) {
        mixBuffer = state.persistBuffer;
        state.persistBufferRemaining = state.bufferSize;
        memset(mixBuffer, 0, state.bufferSize*sizeof(float)*OUTPUT_CHANNELS);
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
    ma_mutex_unlock(&state.playbackLock);

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
  if (state.audiotapWriting)
    lovrFilesystemWrite("lovrDebugAudio.raw", outputUntyped, originalCount*OUTPUT_CHANNELS*sizeof(float), true);
#endif
}

static void onCapture(ma_device* device, void* outputUntyped, const void* inputUntyped, uint32_t frames) {
  // note: ma_pcm_rb is lockless
  const float *input = inputUntyped;
  void *store;
  size_t bytesPerFrame = SampleFormatBytesPerFrame(CAPTURE_CHANNELS, OUTPUT_FORMAT);
  while(frames > 0) {
    uint32_t availableFrames = frames;
    ma_result acquire_status = ma_pcm_rb_acquire_write(&state.captureRingbuffer, &availableFrames, &store);
    if (acquire_status != MA_SUCCESS) {
      return;
    }
    memcpy(store, input, availableFrames * bytesPerFrame);
    ma_result commit_status = ma_pcm_rb_commit_write(&state.captureRingbuffer, availableFrames, store);
    if (commit_status != MA_SUCCESS || availableFrames == 0) {
      return;
    }
    frames -= availableFrames;
    input += availableFrames * bytesPerFrame;
  }
}

static const ma_device_callback_proc callbacks[] = { onPlayback, onCapture };

static Spatializer *spatializers[] = {
#ifdef LOVR_ENABLE_OCULUS_AUDIO
  &oculusSpatializer,
#endif
  &dummySpatializer,
};

// Entry

bool lovrAudioInit(AudioConfig config, AudioDeviceConfig deviceConfig[2]) {
  if (state.initialized) return false;

  memcpy(state.deviceConfig, deviceConfig, sizeof(state.deviceConfig));

  if (ma_context_init(NULL, 0, NULL, &state.context)) {
    return false;
  }

  int mutexStatus = ma_mutex_init(&state.playbackLock);
  lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");

  lovrAudioReset();

  SpatializerConfigIn spatializerConfigIn = {
    .maxSourcesHint = config.spatializerMaxSourcesHint,
    .fixedBuffer=CALLBACK_LENGTH,
    .sampleRate=LOVR_AUDIO_SAMPLE_RATE
  };
  SpatializerConfigOut spatializerConfigOut = {0};
  // Find first functioning spatializer
  for (size_t i = 0; i < sizeof(spatializers) / sizeof(spatializers[0]); i++) {
    if (config.spatializer && strcmp(config.spatializer, spatializers[i]->name))
      continue; // If a name was provided, only match spatializers with that exact name
    if (spatializers[i]->init(spatializerConfigIn, &spatializerConfigOut)) {
      state.spatializer = spatializers[i];
      break;
    }
  }
  lovrAssert(state.spatializer != NULL, "Must have at least one spatializer");

  state.fixedBuffer = spatializerConfigOut.needFixedBuffer;
  state.bufferSize = state.fixedBuffer ? CALLBACK_LENGTH : 1024;
  state.scratchBuffer1 = malloc(state.bufferSize*sizeof(float)*OUTPUT_CHANNELS);
  state.scratchBuffer2 = malloc(state.bufferSize*sizeof(float)*OUTPUT_CHANNELS);
  if (state.fixedBuffer)
    state.persistBuffer = malloc(state.bufferSize*sizeof(float)*OUTPUT_CHANNELS);
  state.persistBufferRemaining = 0;

  arr_init(&state.converters);

  // FIXME: This starts the audio. This is a recent change. Should this happen so early?
  for (int i = 0; i < AUDIO_TYPE_COUNT; i++) {
    if (deviceConfig[i].enable && deviceConfig[i].start) {
      int startStatus = ma_device_start(&state.devices[i]);
      if(startStatus != MA_SUCCESS) {
        lovrAudioDestroy();
        lovrAssert(false, "Failed to start audio device %d\n", i);
        return false;
      }
    }
  }

  ma_result rbstatus = ma_pcm_rb_init(miniAudioFormatFromLovr[OUTPUT_FORMAT], CAPTURE_CHANNELS, LOVR_AUDIO_SAMPLE_RATE * 1.0, NULL, NULL, &state.captureRingbuffer);
  if (rbstatus != MA_SUCCESS) {
    lovrAudioDestroy();
    return false;
  }

#ifdef LOVR_DEBUG_AUDIOTAP
  lovrFilesystemWrite("lovrDebugAudio.raw", NULL, 0, false); // Erase file
  state.audiotapWriting = true;
#endif

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.devices[AUDIO_PLAYBACK]);
  ma_device_uninit(&state.devices[AUDIO_CAPTURE]);
  ma_mutex_uninit(&state.playbackLock);
  ma_context_uninit(&state.context);
  if (state.spatializer) state.spatializer->destroy();
  for(int i = 0; i < state.converters.length; i++) {
    ma_data_converter_uninit(state.converters.data[i]);
    free(state.converters.data[i]);
  }
  arr_free(&state.converters);
  memset(&state, 0, sizeof(state));
  free(state.scratchBuffer1); state.scratchBuffer1 = NULL;
  free(state.scratchBuffer2); state.scratchBuffer2 = NULL;
  free(state.persistBuffer);  state.persistBuffer = NULL;

#ifdef LOVR_DEBUG_AUDIOTAP
  state.audiotapWriting = false;
#endif
}

bool lovrAudioInitDevice(AudioType type) {
  ma_device_type deviceType = (type == AUDIO_PLAYBACK) ? ma_device_type_playback : ma_device_type_capture;

  ma_device_config config = ma_device_config_init(deviceType);
  config.sampleRate = LOVR_AUDIO_SAMPLE_RATE;
  config.playback.format = miniAudioFormatFromLovr[OUTPUT_FORMAT];
  config.capture.format = miniAudioFormatFromLovr[OUTPUT_FORMAT];
  config.playback.channels = OUTPUT_CHANNELS;
  config.capture.channels = CAPTURE_CHANNELS;
  config.dataCallback = callbacks[type];
  config.periodSizeInFrames = PERIOD_LENGTH;
  config.periods = 3;
  config.performanceProfile = ma_performance_profile_low_latency;

  int err = ma_device_init(&state.context, &config, &state.devices[type]); 
  if (err != MA_SUCCESS) {
    lovrLog(LOG_WARN, "audio", "Failed to enable audio device %d: %d\n", type, err);
    return false;
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
    if (state.deviceConfig[i].enable) {
      lovrAudioInitDevice(i);
    }
  }

  return true;
}

bool lovrAudioStart(AudioType type) {
  if (state.deviceConfig[type].enable == false) {
    if (lovrAudioInitDevice(type) == false) {
      if (type == AUDIO_CAPTURE) {
        lovrPlatformRequestPermission(AUDIO_CAPTURE_PERMISSION);
        // lovrAudioStart will be retried from boot.lua upon permission granted event
      }
      return false;
    }
    state.deviceConfig[type].enable = state.deviceConfig[type].start = true;
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

void lovrAudioSetListenerPose(float position[4], float orientation[4])
{
  state.spatializer->setListenerPose(position, orientation);
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
    config.sampleRateOut = LOVR_AUDIO_SAMPLE_RATE;
    
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
  _lovrSourceAssignConverter(source);
  state.spatializer->sourceCreate(source);

  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  state.spatializer->sourceDestroy(source);
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
  memcpy(source->position, position, sizeof(source->position));
  memcpy(source->orientation, orientation, sizeof(source->orientation));
  ma_mutex_unlock(&state.playbackLock);
}

void lovrSourceGetPose(Source *source, float position[4], float orientation[4]) {
  memcpy(position, source->position, sizeof(source->position));
  memcpy(orientation, source->orientation, sizeof(source->orientation));
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

uint32_t lovrAudioGetCaptureSampleCount() {
  // note: must only be called from ONE thread!! ma_pcm_rb only promises
  // thread safety with ONE reader and ONE writer thread.
  return ma_pcm_rb_available_read(&state.captureRingbuffer);
}

static const char *format2string(SampleFormat f) { return f == SAMPLE_I16 ? "i16" : "f32"; }

struct SoundData* lovrAudioCapture(uint32_t frameCount, SoundData *soundData, uint32_t offset) {

  uint32_t bufferedFrames = lovrAudioGetCaptureSampleCount();
  if (frameCount == 0 || frameCount > bufferedFrames) {
    frameCount = bufferedFrames;
  }

  if (frameCount == 0) {
    return NULL;
  }

  if (soundData == NULL) {
    soundData = lovrSoundDataCreateRaw(frameCount, CAPTURE_CHANNELS, LOVR_AUDIO_SAMPLE_RATE, OUTPUT_FORMAT, NULL);
  } else {
    lovrAssert(soundData->channels == CAPTURE_CHANNELS, "Capture (%d) and SoundData (%d) channel counts must match", CAPTURE_CHANNELS, soundData->channels);
    lovrAssert(soundData->sampleRate == LOVR_AUDIO_SAMPLE_RATE, "Capture (%d) and SoundData (%d) sample rates must match", LOVR_AUDIO_SAMPLE_RATE, soundData->sampleRate);
    lovrAssert(soundData->format == OUTPUT_FORMAT, "Capture (%s) and SoundData (%s) formats must match", format2string(OUTPUT_FORMAT), format2string(soundData->format));
    lovrAssert(offset + frameCount <= soundData->frames, "Tried to write samples past the end of a SoundData buffer");
  }

  uint32_t bytesPerFrame = SampleFormatBytesPerFrame(CAPTURE_CHANNELS, OUTPUT_FORMAT);
  while(frameCount > 0) {
    uint32_t availableFramesInRB = frameCount;
    void *store;
    ma_result acquire_status = ma_pcm_rb_acquire_read(&state.captureRingbuffer, &availableFramesInRB, &store);
    if (acquire_status != MA_SUCCESS) {
      lovrAssert(false, "Failed to acquire ring buffer for read: %d\n", acquire_status);
      return NULL;
    }
    memcpy(((unsigned char *)soundData->blob->data) + offset * bytesPerFrame, store, availableFramesInRB * bytesPerFrame);
    ma_result commit_status = ma_pcm_rb_commit_read(&state.captureRingbuffer, availableFramesInRB, store);
    if (commit_status != MA_SUCCESS) {
      lovrAssert(false, "Failed to commit ring buffer for read: %d\n", acquire_status);
      return NULL;
    }
    frameCount -= availableFramesInRB;
    offset += availableFramesInRB;
  }

  return soundData;
}

intptr_t *lovrSourceGetSpatializerMemoField(Source *source) {
  return &source->spatializerMemo;
}

const char *lovrSourceGetSpatializerName() {
  return state.spatializer->name;
}
