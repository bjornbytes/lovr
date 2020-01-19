#include "audio/microphone.h"
#include "audio/audio.h"
#include "data/soundDataPool.h"
#include "core/ref.h"
#include "core/util.h"
#include <AL/al.h>
#include <AL/alc.h>

struct Microphone {
  ALCdevice* device;
  const char* name;
  bool isRecording;
  uint32_t sampleRate;
  uint32_t bitDepth;
  uint32_t channelCount;
  SoundDataPool* pool;
};

Microphone* lovrMicrophoneCreate(const char* name, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount) {
  Microphone* microphone = lovrAlloc(Microphone);
  ALCdevice* device = alcCaptureOpenDevice(name, sampleRate, lovrAudioConvertFormat(bitDepth, channelCount), (ALCsizei) samples);
  lovrAssert(device, "Error opening capture device for microphone '%s'", name);
  microphone->device = device;
  microphone->name = name ? name : alcGetString(device, ALC_CAPTURE_DEVICE_SPECIFIER);
  microphone->sampleRate = sampleRate;
  microphone->bitDepth = bitDepth;
  microphone->channelCount = channelCount;
  return microphone;
}

void lovrMicrophoneDestroy(void* ref) {
  Microphone* microphone = ref;
  lovrMicrophoneStopRecording(microphone);
  lovrRelease(SoundDataPool, microphone->pool);
  alcCaptureCloseDevice(microphone->device);
}

uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone) {
  return microphone->bitDepth;
}

uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone) {
  return microphone->channelCount;
}

SoundData* lovrMicrophoneGetData(Microphone* microphone, size_t samples) {
  if (!microphone->isRecording) {
    return NULL;
  }

  bool usePool = true;
  size_t availableSamples = lovrMicrophoneGetSampleCount(microphone);
  if (availableSamples == 0) {
    return NULL;
  }
  lovrAssert(samples <= availableSamples, "Requested more audio data than is buffered by the microphone");
  if (samples == 0) {
    samples = availableSamples;
    // if client code is just getting the full ringbuffer every time, sample size will differ every time and
    // pool will just make malloc churn worse, not better.
    usePool = false;
  }

  SoundData* soundData = NULL;
  if (usePool) {
    if (microphone->pool && microphone->pool->samples != samples) {
      lovrRelease(SoundDataPool, microphone->pool);
      microphone->pool = NULL;
    }
    if (!microphone->pool) {
      microphone->pool = lovrSoundDataPoolCreate(samples, microphone->sampleRate, microphone->bitDepth, microphone->channelCount);
    }
    soundData = lovrSoundDataPoolCreateSoundData(microphone->pool);
  } else {
    soundData = lovrSoundDataCreate(samples, microphone->sampleRate, microphone->bitDepth, microphone->channelCount);
  }

  alcCaptureSamples(microphone->device, soundData->blob->data, (ALCsizei) samples);
  return soundData;
}

const char* lovrMicrophoneGetName(Microphone* microphone) {
  return microphone->name;
}

size_t lovrMicrophoneGetSampleCount(Microphone* microphone) {
  if (!microphone->isRecording) {
    return 0;
  }

  ALCint samples;
  alcGetIntegerv(microphone->device, ALC_CAPTURE_SAMPLES, sizeof(ALCint), &samples);
  return (size_t) samples;
}

uint32_t lovrMicrophoneGetSampleRate(Microphone* microphone) {
  return microphone->sampleRate;
}

bool lovrMicrophoneIsRecording(Microphone* microphone) {
  return microphone->isRecording;
}

void lovrMicrophoneStartRecording(Microphone* microphone) {
  if (microphone->isRecording) {
    return;
  }

  alcCaptureStart(microphone->device);
  microphone->isRecording = true;
}

void lovrMicrophoneStopRecording(Microphone* microphone) {
  if (!microphone->isRecording) {
    return;
  }

  alcCaptureStop(microphone->device);
  microphone->isRecording = false;
}
