#include "data/audioStream.h"
#include "data/soundData.h"
#include "util.h"
#ifdef USE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <stdbool.h>

#pragma once

#define SOURCE_BUFFERS 4

typedef enum {
  SOURCE_STATIC,
  SOURCE_STREAM
} SourceType;

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

#ifdef USE_OPENAL
typedef struct {
  Ref ref;
  SourceType type;
  SoundData* soundData;
  AudioStream* stream;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  bool isLooping;
} Source;
#else
typedef struct { Ref ref; } Source;
typedef unsigned int ALuint;
#define ALC_HRTF_SOFT 0
#endif

Source* lovrSourceCreateStatic(SoundData* soundData);
Source* lovrSourceCreateStream(AudioStream* stream);
void lovrSourceDestroy(void* ref);
SourceType lovrSourceGetType(Source* source);
int lovrSourceGetBitDepth(Source* source);
int lovrSourceGetChannelCount(Source* source);
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
bool lovrSourceIsLooping(Source* source);
bool lovrSourceIsPaused(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsRelative(Source* source);
bool lovrSourceIsStopped(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
void lovrSourceResume(Source* source);
void lovrSourceRewind(Source* source);
void lovrSourceSeek(Source* source, int sample);
void lovrSourceSetCone(Source* source, float inner, float outer, float outerGain);
void lovrSourceSetDirection(Source* source, float x, float y, float z);
void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff);
void lovrSourceSetLooping(Source* source, bool isLooping);
void lovrSourceSetPitch(Source* source, float pitch);
void lovrSourceSetPosition(Source* source, float x, float y, float z);
void lovrSourceSetRelative(Source* source, bool isRelative);
void lovrSourceSetVelocity(Source* source, float x, float y, float z);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceSetVolumeLimits(Source* source, float min, float max);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, ALuint* buffers, int count);
int lovrSourceTell(Source* source);
