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

SoundData* lovrSoundDataCreate(int samples, int sampleRate, int bitDepth, int channels);
SoundData* lovrSoundDataCreateFromAudioStream(AudioStream* audioStream);
SoundData* lovrSoundDataCreateFromBlob(Blob* blob);
float lovrSoundDataGetSample(SoundData* soundData, int index);
void lovrSoundDataSetSample(SoundData* soundData, int index, float value);
void lovrSoundDataDestroy(void* ref);
