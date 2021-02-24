#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;

// Can pass as the maxFrames argument to lovrSoundCreateFromCallback
#define LOVR_SOUND_ENDLESS 0xFFFFFFFF

typedef enum {
  SAMPLE_F32,
  SAMPLE_I16
} SampleFormat;

typedef enum {
  CHANNEL_MONO,
  CHANNEL_STEREO,
  CHANNEL_AMBISONIC
} ChannelLayout;

typedef struct Sound Sound;

typedef uint32_t (SoundCallback)(Sound* sound, uint32_t offset, uint32_t count, void* data);
typedef void (SoundDestroyCallback)(Sound* sound);

Sound* lovrSoundCreateRaw(uint32_t frames, SampleFormat format, ChannelLayout channels, uint32_t sampleRate, struct Blob* data);
Sound* lovrSoundCreateStream(uint32_t frames, SampleFormat format, ChannelLayout channels, uint32_t sampleRate);
Sound* lovrSoundCreateFromFile(struct Blob* blob, bool decode);
Sound* lovrSoundCreateFromCallback(SoundCallback read, void *callbackMemo, SoundDestroyCallback callbackDataDestroy, SampleFormat format, uint32_t sampleRate, ChannelLayout channels, uint32_t maxFrames);
void lovrSoundDestroy(void* ref);
struct Blob* lovrSoundGetBlob(Sound* sound);
SampleFormat lovrSoundGetFormat(Sound* sound);
ChannelLayout lovrSoundGetChannelLayout(Sound* sound);
uint32_t lovrSoundGetChannelCount(Sound* sound);
uint32_t lovrSoundGetSampleRate(Sound* sound);
uint32_t lovrSoundGetFrameCount(Sound* sound);
size_t lovrSoundGetStride(Sound* sound);
bool lovrSoundIsCompressed(Sound* sound);
bool lovrSoundIsStream(Sound* sound);
uint32_t lovrSoundRead(Sound* sound, uint32_t offset, uint32_t count, void* data);
uint32_t lovrSoundWrite(Sound* sound, uint32_t offset, uint32_t count, const void* data);
uint32_t lovrSoundCopy(Sound* src, Sound* dst, uint32_t frames, uint32_t srcOffset, uint32_t dstOffset);
void *lovrSoundGetCallbackMemo(Sound *sound);
