#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#ifndef LOVR_AUDIO_TYPES
#define LOVR_AUDIO_TYPES

typedef struct {
  ALCdevice* device;
  ALCcontext* context;
} AudioState;

#endif

void lovrAudioInit();
void lovrAudioDestroy();
