#include "util.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#ifndef LOVR_SOURCE_TYPES
#define LOVR_SOURCE_TYPES

#define SOURCE_BUFFERS 4

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

typedef struct {
  int bitDepth;
  int channels;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
} SoundData;

typedef struct {
  Ref ref;
  SoundData* soundData;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  int isLooping;
} Source;

#endif

Source* lovrSourceCreate(SoundData* soundData);
void lovrSourceDestroy(const Ref* ref);
int lovrSourceGetBitDepth(Source* source);
int lovrSourceGetChannels(Source* source);
int lovrSourceGetDuration(Source* source);
ALenum lovrSourceGetFormat(Source* source);
float lovrSourceGetPitch(Source* source);
void lovrSourceGetPosition(Source* source, float* x, float* y, float* z);
int lovrSourceGetSampleRate(Source* source);
float lovrSourceGetVolume(Source* source);
int lovrSourceIsLooping(Source* source);
int lovrSourceIsPaused(Source* source);
int lovrSourceIsPlaying(Source* source);
int lovrSourceIsStopped(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
void lovrSourceResume(Source* source);
void lovrSourceRewind(Source* source);
void lovrSourceSeek(Source* source, int sample);
void lovrSourceSetLooping(Source* source, int isLooping);
void lovrSourceSetPitch(Source* source, float pitch);
void lovrSourceSetPosition(Source* source, float x, float y, float z);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, ALuint* buffers, int count);
int lovrSourceTell(Source* source);
