#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16
} SampleFormat;

typedef struct SoundData SoundData;
SoundData* lovrSoundDataCreateRaw(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate, struct Blob* data);
SoundData* lovrSoundDataCreateStream(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate);
SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode);
void lovrSoundDataDestroy(void* ref);
struct Blob* lovrSoundDataGetBlob(SoundData* soundData);
SampleFormat lovrSoundDataGetFormat(SoundData* soundData);
uint32_t lovrSoundDataGetChannelCount(SoundData* soundData);
uint32_t lovrSoundDataGetSampleRate(SoundData* soundData);
uint32_t lovrSoundDataGetFrameCount(SoundData* soundData);
size_t lovrSoundDataGetStride(SoundData* soundData);
bool lovrSoundDataIsCompressed(SoundData* soundData);
bool lovrSoundDataIsStream(SoundData* soundData);
uint32_t lovrSoundDataRead(SoundData* soundData, uint32_t offset, uint32_t count, void* data);
uint32_t lovrSoundDataWrite(SoundData* soundData, uint32_t offset, uint32_t count, const void* data);
uint32_t lovrSoundDataCopy(SoundData* src, SoundData* dst, uint32_t frames, uint32_t srcOffset, uint32_t dstOffset);
