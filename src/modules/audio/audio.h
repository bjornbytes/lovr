#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "modules/data/soundData.h"

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

typedef void* AudioDeviceIdentifier;

typedef struct {
  bool enable;
  bool start;
  AudioDeviceIdentifier device;
  int sampleRate;
  SampleFormat format;
} AudioConfig;

typedef struct {
  AudioType type;
  const char *name;
  bool isDefault;
  AudioDeviceIdentifier identifier;
  int minChannels, maxChannels;
} AudioDevice;

bool lovrAudioInit(AudioConfig config[2]);
void lovrAudioDestroy(void);
bool lovrAudioReset(void);
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

void lovrAudioGetDevices(AudioDevice **outDevices, size_t *outCount);
void lovrAudioUseDevice(AudioDeviceIdentifier identifier, int sampleRate, SampleFormat format);