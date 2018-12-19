#include "data/blob.h"
#include "data/audioStream.h"

#pragma once

typedef struct {
  Blob blob;
  int channelCount;
  int sampleRate;
  int samples;
  int bitDepth;
} SoundData;

SoundData* lovrSoundDataInit(SoundData* soundData, int samples, int sampleRate, int bitDepth, int channels);
SoundData* lovrSoundDataInitFromAudioStream(SoundData* soundData, AudioStream* audioStream);
SoundData* lovrSoundDataInitFromBlob(SoundData* soundData, Blob* blob);
#define lovrSoundDataCreate(...) lovrSoundDataInit(lovrAlloc(SoundData, lovrSoundDataDestroy), __VA_ARGS__)
#define lovrSoundDataCreateFromAudioStream(...) lovrSoundDataInitFromAudioStream(lovrAlloc(SoundData, lovrSoundDataDestroy), __VA_ARGS__)
#define lovrSoundDataCreateFromBlob(...) lovrSoundDataInitFromBlob(lovrAlloc(SoundData, lovrSoundDataDestroy), __VA_ARGS__)
float lovrSoundDataGetSample(SoundData* soundData, int index);
void lovrSoundDataSetSample(SoundData* soundData, int index, float value);
void lovrSoundDataDestroy(void* ref);
