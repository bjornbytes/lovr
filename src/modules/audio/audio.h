#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "modules/data/soundData.h"
#include "core/arr.h"

#pragma once

struct SoundData;

typedef struct Source Source;

typedef enum {
  AUDIO_PLAYBACK,
  AUDIO_CAPTURE
} AudioType;

typedef enum {
  SOURCE_STATIC,
  SOURCE_STREAM
} SourceType;

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
bool lovrAudioIsRunning(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioSetListenerPose(float position[4], float orientation[4]);
struct SoundData* lovrAudioGetCaptureStream(void);
AudioDeviceArr* lovrAudioGetDevices(AudioType type);
void lovrAudioFreeDevices(AudioDeviceArr* devices);
void lovrAudioUseDevice(AudioType type, const char* deviceName);
void lovrAudioSetCaptureFormat(SampleFormat format, int sampleRate);
const char* lovrSourceGetSpatializerName();

// Source

Source* lovrSourceCreate(struct SoundData* soundData, bool spatial);
void lovrSourceDestroy(void* ref);
void lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool isLooping);
float lovrSourceGetVolume(Source* source);
void lovrSourceSetVolume(Source* source, float volume);
bool lovrSourceGetSpatial(Source *source);
void lovrSourceSetPose(Source *source, float position[4], float orientation[4]);
void lovrSourceGetPose(Source *source, float position[4], float orientation[4]);
uint32_t lovrSourceGetTime(Source* source);
void lovrSourceSetTime(Source* source, uint32_t sample);
struct SoundData* lovrSourceGetSoundData(Source* source);
intptr_t *lovrSourceGetSpatializerMemoField(Source* source);
