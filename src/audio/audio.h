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
void lovrAudioPlay(Source* source);
