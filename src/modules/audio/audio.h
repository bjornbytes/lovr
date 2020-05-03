#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_MICROPHONES 8

struct AudioStream;
struct SoundData;

typedef struct Source Source;
typedef struct Microphone Microphone;

typedef enum {
  SOURCE_STATIC,
  SOURCE_STREAM
} SourceType;

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

bool lovrAudioInit(void);
void lovrAudioDestroy(void);
void lovrAudioUpdate(void);
void lovrAudioAdd(struct Source* source);
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound);
void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint32_t* count);
void lovrAudioGetOrientation(float* orientation);
void lovrAudioGetPosition(float* position);
void lovrAudioGetVelocity(float* velocity);
float lovrAudioGetVolume(void);
bool lovrAudioHas(struct Source* source);
bool lovrAudioIsSpatialized(void);
void lovrAudioPause(void);
void lovrAudioSetDopplerEffect(float factor, float speedOfSound);
void lovrAudioSetOrientation(float* orientation);
void lovrAudioSetPosition(float* position);
void lovrAudioSetVelocity(float* velocity);
void lovrAudioSetVolume(float volume);
void lovrAudioStop(void);

Source* lovrSourceCreateStatic(struct SoundData* soundData);
Source* lovrSourceCreateStream(struct AudioStream* stream);
void lovrSourceDestroy(void* ref);
SourceType lovrSourceGetType(Source* source);
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
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsRelative(Source* source);
void lovrSourcePause(Source* source);
void lovrSourcePlay(Source* source);
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

Microphone* lovrMicrophoneCreate(const char* name, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount);
void lovrMicrophoneDestroy(void* ref);
uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone);
uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone);
struct SoundData* lovrMicrophoneGetData(Microphone* microphone, size_t samples, struct SoundData* soundData, size_t offset);
const char* lovrMicrophoneGetName(Microphone* microphone);
size_t lovrMicrophoneGetSampleCount(Microphone* microphone);
uint32_t lovrMicrophoneGetSampleRate(Microphone* microphone);
bool lovrMicrophoneIsRecording(Microphone* microphone);
void lovrMicrophoneStartRecording(Microphone* microphone);
void lovrMicrophoneStopRecording(Microphone* microphone);
