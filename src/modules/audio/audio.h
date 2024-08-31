#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

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
  EFFECT_TRANSMISSION
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

typedef struct {
  size_t idSize;
  const void* id;
  const char* name;
  bool isDefault;
} AudioDevice;

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

typedef void AudioDeviceCallback(AudioDevice* device, void* userdata);

bool lovrAudioInit(const char* spatializer, uint32_t sampleRate);
void lovrAudioDestroy(void);
void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata);
bool lovrAudioGetDevice(AudioType type, AudioDevice* device);
bool lovrAudioSetDevice(AudioType type, void* id, size_t size, struct Sound* sink, AudioShareMode shareMode);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
bool lovrAudioIsStarted(AudioType type);
float lovrAudioGetVolume(VolumeUnit units);
void lovrAudioSetVolume(float volume, VolumeUnit units);
void lovrAudioGetPose(float position[3], float orientation[4]);
void lovrAudioSetPose(float position[3], float orientation[4]);
bool lovrAudioSetGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material);
const char* lovrAudioGetSpatializer(void);
uint32_t lovrAudioGetSampleRate(void);
void lovrAudioGetAbsorption(float absorption[3]);
void lovrAudioSetAbsorption(float absorption[3]);

// Source

Source* lovrSourceCreate(struct Sound* sound, bool pitch, bool spatial, uint32_t effects);
Source* lovrSourceClone(Source* source);
void lovrSourceDestroy(void* ref);
struct Sound* lovrSourceGetSound(Source* source);
bool lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
bool lovrSourceSetLooping(Source* source, bool loop);
float lovrSourceGetPitch(Source* source);
bool lovrSourceSetPitch(Source* source, float pitch);
float lovrSourceGetVolume(Source* source, VolumeUnit units);
void lovrSourceSetVolume(Source* source, float volume, VolumeUnit units);
void lovrSourceSeek(Source* source, double time, TimeUnit units);
double lovrSourceTell(Source* source, TimeUnit units);
double lovrSourceGetDuration(Source* source, TimeUnit units);
bool lovrSourceIsPitchable(Source* source);
bool lovrSourceIsSpatial(Source* source);
void lovrSourceGetPose(Source* source, float position[3], float orientation[4]);
void lovrSourceSetPose(Source* source, float position[3], float orientation[4]);
float lovrSourceGetRadius(Source* source);
void lovrSourceSetRadius(Source* source, float radius);
void lovrSourceGetDirectivity(Source* source, float* weight, float* power);
void lovrSourceSetDirectivity(Source* source, float weight, float power);
bool lovrSourceIsEffectEnabled(Source* source, Effect effect);
bool lovrSourceSetEffectEnabled(Source* Source, Effect effect, bool enabled);
