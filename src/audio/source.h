#include "loaders/source.h"
#include "util.h"
#include <AL/al.h>
#include <AL/alc.h>

#pragma once

#define SOURCE_BUFFERS 4

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

typedef struct {
  Ref ref;
  SourceData* sourceData;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  int isLooping;
} Source;

Source* lovrSourceCreate(SourceData* sourceData);
void lovrSourceDestroy(const Ref* ref);
int lovrSourceGetBitDepth(Source* source);
int lovrSourceGetChannels(Source* source);
void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain);
void lovrSourceGetDirection(Source* source, float* x, float* y, float* z);
int lovrSourceGetDuration(Source* source);
void lovrSourceGetFalloff(Source* source, float* reference, float* max, float* rolloff);
float lovrSourceGetPitch(Source* source);
void lovrSourceGetPosition(Source* source, float* x, float* y, float* z);
void lovrSourceGetVelocity(Source* source, float* x, float* y, float* z);
int lovrSourceGetSampleRate(Source* source);
float lovrSourceGetVolume(Source* source);
void lovrSourceGetVolumeLimits(Source* source, float* min, float* max);
int lovrSourceIsLooping(Source* source);
int lovrSourceIsPaused(Source* source);
int lovrSourceIsPlaying(Source* source);
int lovrSourceIsRelative(Source* source);
int lovrSourceIsStopped(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
void lovrSourceResume(Source* source);
void lovrSourceRewind(Source* source);
void lovrSourceSeek(Source* source, int sample);
void lovrSourceSetCone(Source* source, float inner, float outer, float outerGain);
void lovrSourceSetDirection(Source* source, float x, float y, float z);
void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff);
void lovrSourceSetLooping(Source* source, int isLooping);
void lovrSourceSetPitch(Source* source, float pitch);
void lovrSourceSetPosition(Source* source, float x, float y, float z);
void lovrSourceSetRelative(Source* source, int isRelative);
void lovrSourceSetVelocity(Source* source, float x, float y, float z);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceSetVolumeLimits(Source* source, float min, float max);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, ALuint* buffers, int count);
int lovrSourceTell(Source* source);
