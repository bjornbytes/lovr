#include "lib/math.h"
#include "types.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <stdbool.h>

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

typedef struct Source {
  Ref ref;
  SourceType type;
  struct SoundData* soundData;
  struct AudioStream* stream;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  bool isLooping;
} Source;

Source* lovrSourceInitStatic(Source* source, struct SoundData* soundData);
Source* lovrSourceInitStream(Source* source, struct AudioStream* stream);
#define lovrSourceCreateStatic(...) lovrSourceInitStatic(lovrAlloc(Source), __VA_ARGS__)
#define lovrSourceCreateStream(...) lovrSourceInitStream(lovrAlloc(Source), __VA_ARGS__)
void lovrSourceDestroy(void* ref);
SourceType lovrSourceGetType(Source* source);
int lovrSourceGetBitDepth(Source* source);
int lovrSourceGetChannelCount(Source* source);
void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain);
void lovrSourceGetOrientation(Source* source, quat orientation);
int lovrSourceGetDuration(Source* source);
void lovrSourceGetFalloff(Source* source, float* reference, float* max, float* rolloff);
float lovrSourceGetPitch(Source* source);
void lovrSourceGetPosition(Source* source, vec3 position);
void lovrSourceGetVelocity(Source* source, vec3 velocity);
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
void lovrSourceSetOrientation(Source* source, quat orientation);
void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff);
void lovrSourceSetLooping(Source* source, bool isLooping);
void lovrSourceSetPitch(Source* source, float pitch);
void lovrSourceSetPosition(Source* source, vec3 position);
void lovrSourceSetRelative(Source* source, bool isRelative);
void lovrSourceSetVelocity(Source* source, vec3 velocity);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceSetVolumeLimits(Source* source, float min, float max);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, ALuint* buffers, int count);
int lovrSourceTell(Source* source);
