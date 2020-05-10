#include <stdbool.h>
#include <stdint.h>

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

typedef enum {
  UNIT_SECONDS,
  UNIT_SAMPLES
} TimeUnit;

typedef struct {
  bool enable;
  bool start;
} AudioConfig;

bool lovrAudioInit(AudioConfig config[2]);
void lovrAudioDestroy(void);
bool lovrAudioReset(void);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);

Source* lovrSourceCreate(struct SoundData* soundData);
void lovrSourceDestroy(void* ref);
void lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool isLooping);
float lovrSourceGetVolume(Source* source);
void lovrSourceSetVolume(Source* source, float volume);
uint32_t lovrSourceGetTime(Source* source);
void lovrSourceSetTime(Source* source, uint32_t sample);
uint32_t lovrSourceGetDuration(Source* source);
