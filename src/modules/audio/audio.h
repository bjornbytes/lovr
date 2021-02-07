#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

typedef void* AudioDeviceIdentifier;

typedef struct {
  bool enable;
  bool start;
  AudioDeviceIdentifier device;
} AudioConfig;

typedef struct {
  AudioType type;
  const char *name;
  bool isDefault;
  AudioDeviceIdentifier identifier;
  int minChannels, maxChannels;
} AudioDevice;

#ifndef LOVR_AUDIO_SAMPLE_RATE
#  define LOVR_AUDIO_SAMPLE_RATE 44100
#endif

bool lovrAudioInit(AudioConfig config[2]);
void lovrAudioDestroy(void);
bool lovrAudioReset(void);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioSetListenerPose(float position[4], float orientation[4]);

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

uint32_t lovrAudioGetCaptureSampleCount();
struct SoundData* lovrAudioCapture(uint32_t sampleCount, struct SoundData *soundData, uint32_t offset);

void lovrAudioGetDevices(AudioDevice **outDevices, size_t *outCount);
void lovrAudioUseDevice(AudioDeviceIdentifier identifier);