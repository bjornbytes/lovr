#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "core/arr.h"

#pragma once

struct Blob;
struct SoundData;

typedef struct AudioStream {
  uint32_t bitDepth;
  uint32_t channelCount;
  uint32_t sampleRate;
  size_t samples; // if raw: count of samples queued in queuedRawBuffers
  size_t bufferSize;
  void* buffer;
  void* decoder; // null if stream is raw
  struct Blob* blob;
  arr_t(struct Blob*) queuedRawBuffers;
  size_t queueLimitInSamples;
  size_t firstBlobCursor; // bytes into queuedRawBuffers.data[0] at which to do the next read
} AudioStream;

AudioStream* lovrAudioStreamInit(AudioStream* stream, struct Blob* blob, size_t bufferSize);
#define lovrAudioStreamCreate(...) lovrAudioStreamInit(lovrAlloc(AudioStream), __VA_ARGS__)
AudioStream* lovrAudioStreamInitRaw(AudioStream* stream, int channelCount, int sampleRate, size_t bufferSize, size_t queueLimitInSamples);
#define lovrAudioStreamCreateRaw(...) lovrAudioStreamInitRaw(lovrAlloc(AudioStream), __VA_ARGS__)
void lovrAudioStreamDestroy(void* ref);
size_t lovrAudioStreamDecode(AudioStream* stream, int16_t* destination, size_t size);
bool lovrAudioStreamAppendRawBlob(AudioStream* stream, struct Blob* blob);
bool lovrAudioStreamAppendRawSound(AudioStream* stream, struct SoundData* sound);
double lovrAudioStreamGetDurationInSeconds(AudioStream* stream);
bool lovrAudioStreamIsRaw(AudioStream* stream);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, size_t sample);
size_t lovrAudioStreamTell(AudioStream* stream);
