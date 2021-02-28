#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define PLAYBACK_SAMPLE_RATE 48000
#define MAX_SOURCES 64

struct Sound;

typedef struct Source Source;

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
  AUDIO_PLAYBACK,
  AUDIO_CAPTURE
} AudioType;

typedef enum {
  UNIT_SECONDS,
  UNIT_FRAMES
} TimeUnit;

typedef enum {
  SOURCE_NEAREST,
  SOURCE_BILINEAR
} SourceInterpolation;

typedef void AudioDeviceCallback(const void* id, size_t size, const char* name, bool isDefault, void* userdata);

bool lovrAudioInit(const char* spatializer);
void lovrAudioDestroy(void);
void lovrAudioEnumerateDevices(AudioType type, AudioDeviceCallback* callback, void* userdata);
bool lovrAudioSetDevice(AudioType type, void* id, size_t size, uint32_t sampleRate, uint32_t format, bool exclusive);
bool lovrAudioStart(AudioType type);
bool lovrAudioStop(AudioType type);
bool lovrAudioIsStarted(AudioType type);
float lovrAudioGetVolume(void);
void lovrAudioSetVolume(float volume);
void lovrAudioGetPose(float position[4], float orientation[4]);
void lovrAudioSetPose(float position[4], float orientation[4]);
bool lovrAudioSetGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material);
const char* lovrAudioGetSpatializer(void);
struct Sound* lovrAudioGetCaptureStream(void);

// Source

Source* lovrSourceCreate(struct Sound* sound, bool spatial);
Source* lovrSourceClone(Source* source);
void lovrSourceDestroy(void* ref);
bool lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
void lovrSourceStop(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool loop);
float lovrSourceGetVolume(Source* source);
void lovrSourceSetVolume(Source* source, float volume);
void lovrSourceSeek(Source* source, double time, TimeUnit units);
double lovrSourceTell(Source* source, TimeUnit units);
double lovrSourceGetDuration(Source* source, TimeUnit units);
bool lovrSourceIsSpatial(Source* source);
SourceInterpolation lovrSourceGetInterpolation(Source* source);
void lovrSourceSetInterpolation(Source* source, SourceInterpolation interpolation);
void lovrSourceGetPose(Source* source, float position[4], float orientation[4]);
void lovrSourceSetPose(Source* source, float position[4], float orientation[4]);
float lovrSourceGetRadius(Source* source);
void lovrSourceSetRadius(Source* source, float radius);
void lovrSourceGetDirectivity(Source* source, float* weight, float* power);
void lovrSourceSetDirectivity(Source* source, float weight, float power);
bool lovrSourceIsAbsorptionEnabled(Source* source);
void lovrSourceSetAbsorptionEnabled(Source* source, bool enabled);
bool lovrSourceIsFalloffEnabled(Source* source);
void lovrSourceSetFalloffEnabled(Source* source, bool enabled);
bool lovrSourceIsOcclusionEnabled(Source* source);
void lovrSourceSetOcclusionEnabled(Source* source, bool enabled);
bool lovrSourceIsReverbEnabled(Source* source);
void lovrSourceSetReverbEnabled(Source* source, bool enabled);
bool lovrSourceIsTransmissionEnabled(Source* source);
void lovrSourceSetTransmissionEnabled(Source* source, bool enabled);
