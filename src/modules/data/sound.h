#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16
} SampleFormat;

typedef struct Sound Sound;
Sound* lovrSoundCreateRaw(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate, struct Blob* data);
Sound* lovrSoundCreateStream(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate);
Sound* lovrSoundCreateFromFile(struct Blob* blob, bool decode);
void lovrSoundDestroy(void* ref);
struct Blob* lovrSoundGetBlob(Sound* sound);
SampleFormat lovrSoundGetFormat(Sound* sound);
uint32_t lovrSoundGetChannelCount(Sound* sound);
uint32_t lovrSoundGetSampleRate(Sound* sound);
uint32_t lovrSoundGetFrameCount(Sound* sound);
size_t lovrSoundGetStride(Sound* sound);
bool lovrSoundIsCompressed(Sound* sound);
bool lovrSoundIsStream(Sound* sound);
uint32_t lovrSoundRead(Sound* sound, uint32_t offset, uint32_t count, void* data);
uint32_t lovrSoundWrite(Sound* sound, uint32_t offset, uint32_t count, const void* data);
uint32_t lovrSoundCopy(Sound* src, Sound* dst, uint32_t frames, uint32_t srcOffset, uint32_t dstOffset);
