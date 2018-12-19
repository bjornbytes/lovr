#include "data/blob.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  int bitDepth;
  int channelCount;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
  Blob* blob;
} AudioStream;

AudioStream* lovrAudioStreamInit(AudioStream* stream, Blob* blob, size_t bufferSize);
#define lovrAudioStreamCreate(...) lovrAudioStreamInit(lovrAlloc(AudioStream, lovrAudioStreamDestroy), __VA_ARGS__)
void lovrAudioStreamDestroy(void* ref);
int lovrAudioStreamDecode(AudioStream* stream, short* destination, size_t size);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, int sample);
int lovrAudioStreamTell(AudioStream* stream);
