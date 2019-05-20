#include "types.h"
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;

typedef struct AudioStream {
  Ref ref;
  uint32_t bitDepth;
  uint32_t channelCount;
  uint32_t sampleRate;
  size_t samples;
  size_t bufferSize;
  void* buffer;
  void* decoder;
  struct Blob* blob;
} AudioStream;

AudioStream* lovrAudioStreamInit(AudioStream* stream, struct Blob* blob, size_t bufferSize);
#define lovrAudioStreamCreate(...) lovrAudioStreamInit(lovrAlloc(AudioStream), __VA_ARGS__)
void lovrAudioStreamDestroy(void* ref);
size_t lovrAudioStreamDecode(AudioStream* stream, int16_t* destination, size_t size);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, size_t sample);
size_t lovrAudioStreamTell(AudioStream* stream);
