#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 256
#define MAX_SOURCES 64

struct Sound;

typedef struct Source Source;

typedef enum {
  EFFECT_ABSORPTION,
  EFFECT_ATTENUATION,
  EFFECT_OCCLUSION,
  EFFECT_REVERB,
  EFFECT_SPATIALIZATION,
  EFFECT_TRANSMISSION,
  EFFECT_ALL = 0x3f,
  EFFECT_NONE = 0xff
} Effect;

typedef enum {
  MATERIAL_GENERIC,
  MATERIAL_BRICK,
  MATERIAL_CARPET,
  MATERIAL_CERAMIC,
  MATERIAL_CONCRETE,
  MATERIAL_GLASS,
  MATERIAL_GRAVEL,
  MATERIAL_METAL,
  MATERIAL_PLASTER,
  MATERIAL_ROCK,
  MATERIAL_WOOD
} AudioMaterial;

typedef enum {
  AUDIO_SHARED,
  AUDIO_EXCLUSIVE
} AudioShareMode;

typedef enum {
  AUDIO_PLAYBACK,
  AUDIO_CAPTURE
} AudioType;

typedef enum {
  UNIT_SECONDS,
  UNIT_FRAMES
} TimeUnit;

typedef enum {
  UNIT_LINEAR,
  UNIT_DECIBELS
} VolumeUnit;

typedef void AudioDeviceCallback(const void* id, size_t size, const char* name, bool isDefault, void* userdata);

bool lovrAudioInit(const char* spatializer);
void lovrAudioDestroy(void);
void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata);
bool lovrAudioSetDevice(AudioType type, void* id, size_t size, struct Sound* sink, AudioShareMode shareMode);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
bool lovrAudioIsStarted(AudioType type);
float lovrAudioGetVolume(VolumeUnit units);
void lovrAudioSetVolume(float volume, VolumeUnit units);
void lovrAudioGetPose(float position[4], float orientation[4]);
void lovrAudioSetPose(float position[4], float orientation[4]);
bool lovrAudioSetGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material);
const char* lovrAudioGetSpatializer(void);
void lovrAudioGetAbsorption(float absorption[3]);
void lovrAudioSetAbsorption(float absorption[3]);

// Source

Source* lovrSourceCreate(struct Sound* sound, uint32_t effects);
Source* lovrSourceClone(Source* source);
void lovrSourceDestroy(void* ref);
struct Sound* lovrSourceGetSound(Source* source);
bool lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool loop);
float lovrSourceGetVolume(Source* source, VolumeUnit units);
void lovrSourceSetVolume(Source* source, float volume, VolumeUnit units);
void lovrSourceSeek(Source* source, double time, TimeUnit units);
double lovrSourceTell(Source* source, TimeUnit units);
double lovrSourceGetDuration(Source* source, TimeUnit units);
bool lovrSourceUsesSpatializer(Source* source);
void lovrSourceGetPose(Source* source, float position[4], float orientation[4]);
void lovrSourceSetPose(Source* source, float position[4], float orientation[4]);
float lovrSourceGetRadius(Source* source);
void lovrSourceSetRadius(Source* source, float radius);
void lovrSourceGetDirectivity(Source* source, float* weight, float* power);
void lovrSourceSetDirectivity(Source* source, float weight, float power);
bool lovrSourceIsEffectEnabled(Source* source, Effect effect);
void lovrSourceSetEffectEnabled(Source* Source, Effect effect, bool enabled);
