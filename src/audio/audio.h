#include "audio/source.h"
#include "audio/microphone.h"
#include "lib/math.h"
#include "lib/vec/vec.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <stdbool.h>

#pragma once

#define MAX_MICROPHONES 8

typedef struct {
  bool initialized;
  ALCdevice* device;
  ALCcontext* context;
  vec_void_t sources;
  bool isSpatialized;
  float orientation[4];
  float position[3];
  float velocity[3];
} AudioState;

ALenum lovrAudioConvertFormat(int bitDepth, int channelCount);

bool lovrAudioInit(void);
void lovrAudioDestroy(void);
void lovrAudioUpdate(void);
void lovrAudioAdd(Source* source);
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound);
void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint8_t* count);
void lovrAudioGetOrientation(quat orientation);
void lovrAudioGetPosition(vec3 position);
void lovrAudioGetVelocity(vec3 velocity);
float lovrAudioGetVolume(void);
bool lovrAudioHas(Source* source);
bool lovrAudioIsSpatialized(void);
void lovrAudioPause(void);
void lovrAudioResume(void);
void lovrAudioRewind(void);
void lovrAudioSetDopplerEffect(float factor, float speedOfSound);
void lovrAudioSetOrientation(quat orientation);
void lovrAudioSetPosition(vec3 position);
void lovrAudioSetVelocity(vec3 velocity);
void lovrAudioSetVolume(float volume);
void lovrAudioStop(void);
