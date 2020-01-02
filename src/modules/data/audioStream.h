#include <stdint.h>
#include <stddef.h>
#include "core/arr.h"

#pragma once

struct Blob;
struct SoundData;

typedef struct AudioStream {
  uint32_t bitDepth;
  uint32_t channelCount;
  uint32_t sampleRate;
  size_t samples;
  size_t bufferSize;
  void* buffer;
  void* decoder; // null if stream is raw
  struct Blob* blob;
  arr_t(struct Blob*) queuedRawBuffers;
} AudioStream;

AudioStream* lovrAudioStreamInit(AudioStream* stream, struct Blob* blob, size_t bufferSize);
#define lovrAudioStreamCreate(...) lovrAudioStreamInit(lovrAlloc(AudioStream), __VA_ARGS__)
AudioStream* lovrAudioStreamInitRaw(AudioStream* stream, size_t bufferSize, int channelCount, int sampleRate);
#define lovrAudioStreamCreateRaw(...) lovrAudioStreamInitRaw(lovrAlloc(AudioStream), __VA_ARGS__)
void lovrAudioStreamDestroy(void* ref);
size_t lovrAudioStreamDecode(AudioStream* stream, int16_t* destination, size_t size);
void lovrAudioStreamAppendRawBlob(AudioStream* stream, struct Blob* blob);
void lovrAudioStreamAppendRawSound(AudioStream* stream, struct SoundData* sound);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, size_t sample);
size_t lovrAudioStreamTell(AudioStream* stream);
