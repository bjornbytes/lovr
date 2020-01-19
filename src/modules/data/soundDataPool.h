#include "data/soundData.h"
#include "core/arr.h"
#include <stdint.h>

#pragma once

typedef struct SoundDataPool {
  arr_t(SoundData*) available;
  arr_t(SoundData*) used;
  size_t samples;
  uint32_t sampleRate;
  uint32_t bitDepth;
  uint32_t channels;
} SoundDataPool;

SoundDataPool* lovrSoundDataPoolInit(SoundDataPool* pool, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channels);
#define lovrSoundDataPoolCreate(...) lovrSoundDataPoolInit(lovrAlloc(SoundDataPool), __VA_ARGS__)
SoundData *lovrSoundDataPoolCreateSoundData(SoundDataPool* pool);
void lovrSoundDataPoolDestroy(void* ref);
