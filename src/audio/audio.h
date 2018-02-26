#include "audio/source.h"
#include "lib/vec/vec.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <stdbool.h>

#pragma once

typedef struct {
  bool initialized;
  ALCdevice* device;
  ALCcontext* context;
  vec_void_t sources;
  bool isSpatialized;
  float orientation[4];
  float position[3];
  float velocity[4];
} AudioState;

void lovrAudioInit();
void lovrAudioDestroy();
void lovrAudioUpdate();
void lovrAudioAdd(Source* source);
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound);
void lovrAudioGetOrientation(float* angle, float* ax, float* ay, float* az);
void lovrAudioGetPosition(float* x, float* y, float* z);
void lovrAudioGetVelocity(float* x, float* y, float* z);
float lovrAudioGetVolume();
bool lovrAudioHas(Source* source);
bool lovrAudioIsSpatialized();
void lovrAudioPause();
void lovrAudioResume();
void lovrAudioRewind();
void lovrAudioSetDopplerEffect(float factor, float speedOfSound);
void lovrAudioSetOrientation(float angle, float ax, float ay, float az);
void lovrAudioSetPosition(float x, float y, float z);
void lovrAudioSetVelocity(float x, float y, float z);
void lovrAudioSetVolume(float volume);
void lovrAudioStop();
