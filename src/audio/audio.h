#include "audio/source.h"
#include "vendor/vec/vec.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#ifndef LOVR_AUDIO_TYPES
#define LOVR_AUDIO_TYPES

typedef struct {
  ALCdevice* device;
  ALCcontext* context;
  vec_void_t sources;
} AudioState;

#endif

void lovrAudioInit();
void lovrAudioDestroy();
void lovrAudioUpdate();
void lovrAudioGetOrientation(float* fx, float* fy, float* fz, float* ux, float* uy, float* uz);
void lovrAudioGetPosition(float* x, float* y, float* z);
void lovrAudioPlay(Source* source);
void lovrAudioSetOrientation(float fx, float fy, float fz, float ux, float uy, float uz);
void lovrAudioSetPosition(float x, float y, float z);
