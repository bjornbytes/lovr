#include "audio/microphone.h"
#include "audio/audio.h"
#include "data/soundData.h"
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
  alcCaptureCloseDevice(microphone->device);
}

uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone) {
  return microphone->bitDepth;
}

uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone) {
  return microphone->channelCount;
}

SoundData* lovrMicrophoneGetData(Microphone* microphone, size_t samples, SoundData* soundData, size_t offset) {
  size_t availableSamples = lovrMicrophoneGetSampleCount(microphone);

  if (!microphone->isRecording || availableSamples == 0) {
    return NULL;
  }

  if (soundData == NULL) {
    soundData = lovrSoundDataCreate(samples, microphone->sampleRate, microphone->bitDepth, microphone->channelCount);
  } else {
    lovrAssert(soundData->channelCount == microphone->channelCount, "Microphone and SoundData channel counts must match");
    lovrAssert(soundData->sampleRate == microphone->sampleRate, "Microphone and SoundData sample rates must match");
    lovrAssert(soundData->bitDepth == microphone->bitDepth, "Microphone and SoundData bit depths must match");
    lovrAssert(offset + samples <= soundData->samples, "Tried to write samples past the end of a SoundData buffer");
  }

  if (samples == 0 || samples > availableSamples) {
    samples = availableSamples;
  }

  uint8_t* data = (uint8_t*) soundData->blob->data + offset * (microphone->bitDepth / 8) * microphone->channelCount;
  alcCaptureSamples(microphone->device, data, (ALCsizei) samples);
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
