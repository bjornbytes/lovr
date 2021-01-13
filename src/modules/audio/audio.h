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
  AUDIO_CAPTURE,

  AUDIO_TYPE_COUNT
} AudioType;

typedef enum {
  SOURCE_STATIC,
  SOURCE_STREAM
} SourceType;

typedef struct {
  AudioType type;
  const char *name;
  bool isDefault;
} AudioDevice;
typedef arr_t(AudioDevice) AudioDeviceArr;

bool lovrAudioInit();
void lovrAudioDestroy(void);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioSetListenerPose(float position[4], float orientation[4]);
double lovrAudioConvertToSeconds(uint32_t sampleCount, AudioType context);

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
uint32_t lovrSourceGetTime(Source* source);
void lovrSourceSetTime(Source* source, uint32_t sample);
struct SoundData* lovrSourceGetSoundData(Source* source);

struct SoundData* lovrAudioGetCaptureStream();

// Return a list of devices for the given type. Must be freed with lovrAudioFreeDevices.
AudioDeviceArr* lovrAudioGetDevices(AudioType type);
// free a list of devices returned from above call
void lovrAudioFreeDevices(AudioDeviceArr *devices);

void lovrAudioSetCaptureFormat(SampleFormat format, int sampleRate);
void lovrAudioUseDevice(AudioType type, const char *deviceName);
