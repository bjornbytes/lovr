#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct SoundData;

typedef struct Microphone Microphone;
Microphone* lovrMicrophoneCreate(const char* name, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount);
void lovrMicrophoneDestroy(void* ref);
uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone);
uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone);
struct SoundData* lovrMicrophoneGetData(Microphone* microphone, size_t samples, struct SoundData* soundData, size_t offset);
const char* lovrMicrophoneGetName(Microphone* microphone);
size_t lovrMicrophoneGetSampleCount(Microphone* microphone);
uint32_t lovrMicrophoneGetSampleRate(Microphone* microphone);
bool lovrMicrophoneIsRecording(Microphone* microphone);
void lovrMicrophoneStartRecording(Microphone* microphone);
void lovrMicrophoneStopRecording(Microphone* microphone);
