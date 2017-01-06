#include "util.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#ifndef LOVR_SOURCE_TYPES
#define LOVR_SOURCE_TYPES

#define SOURCE_BUFFERS 4

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
} Source;

#endif

Source* lovrSourceCreate(SoundData* soundData);
void lovrSourceDestroy(const Ref* ref);
int lovrSourceGetBitDepth(Source* source);
int lovrSourceGetChannels(Source* source);
float lovrSourceGetDuration(Source* source);
ALenum lovrSourceGetFormat(Source* source);
int lovrSourceGetSampleCount(Source* source);
int lovrSourceGetSampleRate(Source* source);
int lovrSourceIsPaused(Source* source);
int lovrSourceIsPlaying(Source* source);
int lovrSourceIsStopped(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
void lovrSourceResume(Source* source);
void lovrSourceRewind(Source* source);
void lovrSourceStop(Source* source);
void lovrSourceStream(Source* source, ALuint* buffers, int count);
