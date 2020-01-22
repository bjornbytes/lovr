#include "data/blob.h"
#include <stdint.h>

#pragma once

struct AudioStream;

typedef struct SoundData {
  Blob* blob;
  uint32_t channelCount;
  uint32_t sampleRate;
  size_t samples;
  uint32_t bitDepth;
} SoundData;

SoundData* lovrSoundDataInit(SoundData* soundData, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channels);
SoundData* lovrSoundDataInitFromAudioStream(SoundData* soundData, struct AudioStream* audioStream);
SoundData* lovrSoundDataInitFromBlob(SoundData* soundData, Blob* blob);
#define lovrSoundDataCreate(...) lovrSoundDataInit(lovrAlloc(SoundData), __VA_ARGS__)
#define lovrSoundDataCreateFromAudioStream(...) lovrSoundDataInitFromAudioStream(lovrAlloc(SoundData), __VA_ARGS__)
#define lovrSoundDataCreateFromBlob(...) lovrSoundDataInitFromBlob(lovrAlloc(SoundData), __VA_ARGS__)
float lovrSoundDataGetSample(SoundData* soundData, size_t index);
void lovrSoundDataSetSample(SoundData* soundData, size_t index, float value);
void lovrSoundDataDestroy(void* ref);
