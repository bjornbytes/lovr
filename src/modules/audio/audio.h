#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "data/soundData.h"
#include "core/arr.h"

#pragma once

struct SoundData;

typedef struct Source Source;

typedef enum {
  AUDIO_PLAYBACK,
  AUDIO_CAPTURE
} AudioType;

typedef struct {
  const char *spatializer;
  int spatializerMaxSourcesHint;
} SpatializerConfig;

typedef struct {
  AudioType type;
  const char *name;
  bool isDefault;
} AudioDevice;

typedef arr_t(AudioDevice) AudioDeviceArr;

bool lovrAudioInit(SpatializerConfig config);
void lovrAudioDestroy(void);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
bool lovrAudioIsStarted(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioGetPose(float position[4], float orientation[4]);
void lovrAudioSetPose(float position[4], float orientation[4]);
struct SoundData* lovrAudioGetCaptureStream(void);
AudioDeviceArr* lovrAudioGetDevices(AudioType type);
void lovrAudioFreeDevices(AudioDeviceArr* devices);
void lovrAudioUseDevice(AudioType type, const char* deviceName);
void lovrAudioSetCaptureFormat(SampleFormat format, uint32_t sampleRate);
const char* lovrAudioGetSpatializer(void);

// Source

Source* lovrSourceCreate(struct SoundData* soundData, bool spatial);
void lovrSourceDestroy(void* ref);
void lovrSourcePlay(Source* source);
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
