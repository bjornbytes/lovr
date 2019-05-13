#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <AL/al.h>
#include <AL/alc.h>

#pragma once

struct SoundData;

typedef struct Microphone {
  Ref ref;
  ALCdevice* device;
  const char* name;
  bool isRecording;
  uint32_t sampleRate;
  uint32_t bitDepth;
  uint32_t channelCount;
} Microphone;

Microphone* lovrMicrophoneInit(Microphone* microphone, const char* name, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount);
#define lovrMicrophoneCreate(...) lovrMicrophoneInit(lovrAlloc(Microphone), __VA_ARGS__)
void lovrMicrophoneDestroy(void* ref);
uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone);
uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone);
struct SoundData* lovrMicrophoneGetData(Microphone* microphone);
const char* lovrMicrophoneGetName(Microphone* microphone);
size_t lovrMicrophoneGetSampleCount(Microphone* microphone);
uint32_t lovrMicrophoneGetSampleRate(Microphone* microphone);
bool lovrMicrophoneIsRecording(Microphone* microphone);
void lovrMicrophoneStartRecording(Microphone* microphone);
void lovrMicrophoneStopRecording(Microphone* microphone);
