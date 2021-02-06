#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define PLAYBACK_SAMPLE_RATE 48000

struct SoundData;

typedef struct Source Source;

typedef enum {
  AUDIO_PLAYBACK,
  AUDIO_CAPTURE
} AudioType;

typedef enum {
  UNIT_SECONDS,
  UNIT_FRAMES
} TimeUnit;

typedef void AudioDeviceCallback(const void* id, size_t size, const char* name, bool isDefault, void* userdata);

bool lovrAudioInit(const char* spatializer);
void lovrAudioDestroy(void);
const char* lovrAudioGetSpatializer(void);
void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata);
bool lovrAudioSetDevice(AudioType type, void* id, size_t size, uint32_t sampleRate, uint32_t format, bool exclusive);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
bool lovrAudioIsStarted(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioGetPose(float position[4], float orientation[4]);
void lovrAudioSetPose(float position[4], float orientation[4]);
struct SoundData* lovrAudioGetCaptureStream(void);

// Source

Source* lovrSourceCreate(struct SoundData* soundData, bool spatial);
void lovrSourceDestroy(void* ref);
bool lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool loop);
float lovrSourceGetVolume(Source* source);
void lovrSourceSetVolume(Source* source, float volume);
bool lovrSourceIsSpatial(Source* source);
void lovrSourceGetPose(Source* source, float position[4], float orientation[4]);
void lovrSourceSetPose(Source* source, float position[4], float orientation[4]);
double lovrSourceGetDuration(Source* source, TimeUnit units);
double lovrSourceGetTime(Source* source, TimeUnit units);
void lovrSourceSetTime(Source* source, double time, TimeUnit units);
intptr_t* lovrSourceGetSpatializerMemoField(Source* source);
