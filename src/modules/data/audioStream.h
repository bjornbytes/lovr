#include "types.h"

#pragma once

struct Blob;

typedef struct AudioStream {
  Ref ref;
  int bitDepth;
  int channelCount;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
  struct Blob* blob;
} AudioStream;

AudioStream* lovrAudioStreamInit(AudioStream* stream, struct Blob* blob, int bufferSize);
#define lovrAudioStreamCreate(...) lovrAudioStreamInit(lovrAlloc(AudioStream), __VA_ARGS__)
void lovrAudioStreamDestroy(void* ref);
int lovrAudioStreamDecode(AudioStream* stream, short* destination, int size);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, int sample);
int lovrAudioStreamTell(AudioStream* stream);
