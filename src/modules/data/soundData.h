#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Blob;
struct SoundData;

typedef uint32_t SoundDataReader(struct SoundData* soundData, uint32_t offset, uint32_t count, void* data);

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16
} SampleFormat;

typedef struct SoundData {
  SoundDataReader* read;
  void* decoder;
  struct Blob* blob;
  struct Blob** queue;
  uint32_t buffers;
  uint32_t head;
  uint32_t tail;
  SampleFormat format;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t frames;
  uint32_t cursor;
} SoundData;

SoundData* lovrSoundDataCreate(uint32_t frames, uint32_t channels, uint32_t sampleRate, SampleFormat format, struct Blob* data, uint32_t buffers);
SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode);
void lovrSoundDataDestroy(void* ref);
