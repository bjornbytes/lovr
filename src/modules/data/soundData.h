#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct SoundData;

typedef uint32_t SoundDataReader(struct SoundData* soundData, uint32_t offset, uint32_t count, void* data);

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16
} SampleFormat;

size_t SampleFormatBytesPerFrame(int channelCount, SampleFormat fmt);

typedef struct SoundData {
  SoundDataReader* read;
  void* decoder;
  struct Blob* blob;
  void* ring;
  SampleFormat format;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t frames;
  uint32_t cursor;
} SoundData;

SoundData* lovrSoundDataCreateRaw(uint32_t frames, uint32_t channels, uint32_t sampleRate, SampleFormat format, struct Blob* data);
SoundData* lovrSoundDataCreateStream(uint32_t bufferSizeInFrames, uint32_t channels, uint32_t sampleRate, SampleFormat format);
SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode);
void lovrSoundDataDestroy(void* ref);
size_t lovrSoundDataStreamAppendBuffer(SoundData* soundData, const void* buffer, size_t byteSize);
size_t lovrSoundDataStreamAppendBlob(SoundData* soundData, struct Blob* blob);
size_t lovrSoundDataStreamAppendSound(SoundData* soundData, SoundData *src);
void lovrSoundDataSetSample(SoundData* soundData, uint32_t index, float value);
uint32_t lovrSoundDataGetFrameCount(SoundData* soundData);
bool lovrSoundDataIsStream(SoundData* soundData);
