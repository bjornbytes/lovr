#include "util.h"
#include "data/soundData.h"
#if USE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <stdbool.h>

#pragma once

#ifdef USE_OPENAL
typedef struct {
  Ref ref;
  ALCdevice* device;
  bool isRecording;
  const char* name;
  int sampleRate;
  int bitDepth;
  int channelCount;
} Microphone;
#else
typedef unsigned int Microphone;
#endif

Microphone* lovrMicrophoneCreate(const char* name, int samples, int sampleRate, int bitDepth, int channelCount);
void lovrMicrophoneDestroy(void* ref);
int lovrMicrophoneGetBitDepth(Microphone* microphone);
int lovrMicrophoneGetChannelCount(Microphone* microphone);
SoundData* lovrMicrophoneGetData(Microphone* microphone);
const char* lovrMicrophoneGetName(Microphone* microphone);
int lovrMicrophoneGetSampleCount(Microphone* microphone);
int lovrMicrophoneGetSampleRate(Microphone* microphone);
bool lovrMicrophoneIsRecording(Microphone* microphone);
void lovrMicrophoneStartRecording(Microphone* microphone);
void lovrMicrophoneStopRecording(Microphone* microphone);
