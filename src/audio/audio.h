#include "audio/source.h"
#include "audio/microphone.h"
#include "lib/vec/vec.h"
#ifdef USE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif
#include <stdbool.h>

#pragma once

#define MAX_MICROPHONES 8

#ifdef USE_OPENAL
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
#else
typedef unsigned int AudioState;
typedef unsigned int ALenum;
#endif

ALenum lovrAudioConvertFormat(int bitDepth, int channelCount);

void lovrAudioInit();
void lovrAudioDestroy();
void lovrAudioUpdate();
void lovrAudioAdd(Source* source);
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound);
void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint8_t* count);
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
