#include "types.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <stdbool.h>

#pragma once

struct SoundData;

typedef struct Microphone {
  Ref ref;
  ALCdevice* device;
  const char* name;
  bool isRecording;
  int sampleRate;
  int bitDepth;
  int channelCount;
} Microphone;

Microphone* lovrMicrophoneInit(Microphone* microphone, const char* name, int samples, int sampleRate, int bitDepth, int channelCount);
#define lovrMicrophoneCreate(...) lovrMicrophoneInit(lovrAlloc(Microphone), __VA_ARGS__)
void lovrMicrophoneDestroy(void* ref);
int lovrMicrophoneGetBitDepth(Microphone* microphone);
int lovrMicrophoneGetChannelCount(Microphone* microphone);
struct SoundData* lovrMicrophoneGetData(Microphone* microphone);
const char* lovrMicrophoneGetName(Microphone* microphone);
int lovrMicrophoneGetSampleCount(Microphone* microphone);
int lovrMicrophoneGetSampleRate(Microphone* microphone);
bool lovrMicrophoneIsRecording(Microphone* microphone);
void lovrMicrophoneStartRecording(Microphone* microphone);
void lovrMicrophoneStopRecording(Microphone* microphone);
