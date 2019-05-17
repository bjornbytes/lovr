#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define SOURCE_BUFFERS 4

struct AudioStream;
struct SoundData;

typedef enum {
  SOURCE_STATIC,
  SOURCE_STREAM
} SourceType;

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

typedef struct Source Source;
Source* lovrSourceCreateStatic(struct SoundData* soundData);
Source* lovrSourceCreateStream(struct AudioStream* stream);
void lovrSourceDestroy(void* ref);
SourceType lovrSourceGetType(Source* source);
uint32_t lovrSourceGetId(Source* source);
struct AudioStream* lovrSourceGetStream(Source* source);
uint32_t lovrSourceGetBitDepth(Source* source);
uint32_t lovrSourceGetChannelCount(Source* source);
void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain);
void lovrSourceGetOrientation(Source* source, float* orientation);
size_t lovrSourceGetDuration(Source* source);
void lovrSourceGetFalloff(Source* source, float* reference, float* max, float* rolloff);
float lovrSourceGetPitch(Source* source);
void lovrSourceGetPosition(Source* source, float* position);
void lovrSourceGetVelocity(Source* source, float* velocity);
uint32_t lovrSourceGetSampleRate(Source* source);
float lovrSourceGetVolume(Source* source);
void lovrSourceGetVolumeLimits(Source* source, float* min, float* max);
bool lovrSourceIsLooping(Source* source);
bool lovrSourceIsPaused(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsRelative(Source* source);
bool lovrSourceIsStopped(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
void lovrSourceResume(Source* source);
void lovrSourceRewind(Source* source);
void lovrSourceSeek(Source* source, size_t sample);
void lovrSourceSetCone(Source* source, float inner, float outer, float outerGain);
void lovrSourceSetOrientation(Source* source, float* orientation);
void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff);
void lovrSourceSetLooping(Source* source, bool isLooping);
void lovrSourceSetPitch(Source* source, float pitch);
void lovrSourceSetPosition(Source* source, float* position);
void lovrSourceSetRelative(Source* source, bool isRelative);
void lovrSourceSetVelocity(Source* source, float* velocity);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceSetVolumeLimits(Source* source, float min, float max);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, uint32_t* buffers, size_t count);
size_t lovrSourceTell(Source* source);
