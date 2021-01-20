#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct SoundData;

typedef uint32_t SoundDataReader(struct SoundData* soundData, uint32_t offset, uint32_t count, void* data);

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16,

  SAMPLE_INVALID
} SampleFormat;

size_t SampleFormatBytesPerFrame(int channelCount, SampleFormat fmt);

typedef struct SoundData {
  SoundDataReader* read;
  void* decoder;
  struct Blob* blob;
  void *ring;  /* ma_pcm_rb */
  SampleFormat format;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t frames;
  uint32_t cursor;
} SoundData;

SoundData* lovrSoundDataCreateRaw(uint32_t frames, uint32_t channels, uint32_t sampleRate, SampleFormat format, struct Blob* data);
SoundData* lovrSoundDataCreateStream(uint32_t bufferSizeInFrames, uint32_t channels, uint32_t sampleRate, SampleFormat format);
SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode);

// returns the number of frames successfully appended (if it's less than the size of blob, the internal ring buffer is full)
size_t lovrSoundDataStreamAppendBlob(SoundData *dest, struct Blob* blob);
size_t lovrSoundDataStreamAppendSound(SoundData *dest, SoundData *src);
void lovrSoundDataSetSample(SoundData* soundData, size_t index, float value);

uint32_t lovrSoundDataGetDuration(SoundData *soundData);

bool lovrSoundDataIsStream(SoundData *soundData);
void lovrSoundDataDestroy(void* ref);
