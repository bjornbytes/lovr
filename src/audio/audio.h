#include "audio/source.h"
#include "vendor/vec/vec.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#ifndef LOVR_AUDIO_TYPES
#define LOVR_AUDIO_TYPES

typedef struct {
  ALCdevice* device;
  ALCcontext* context;
  vec_void_t sources;
  float position[3];
  float orientation[4];
} AudioState;

#endif

void lovrAudioInit();
void lovrAudioDestroy();
void lovrAudioUpdate();
void lovrAudioAdd(Source* source);
void lovrAudioGetOrientation(float* angle, float* ax, float* ay, float* az);
void lovrAudioGetPosition(float* x, float* y, float* z);
float lovrAudioGetVolume();
int lovrAudioHas(Source* source);
void lovrAudioPause();
void lovrAudioResume();
void lovrAudioRewind();
void lovrAudioSetOrientation(float angle, float ax, float ay, float az);
void lovrAudioSetPosition(float x, float y, float z);
void lovrAudioSetVolume(float volume);
void lovrAudioStop();
