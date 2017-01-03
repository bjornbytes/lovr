#include "audio/audio.h"
#include "util.h"
#include <stdlib.h>

static AudioState state;

void lovrAudioInit() {
  ALCdevice* device = alcOpenDevice(NULL);
  if (!device) {
    error("Unable to open default audio device");
  }

  ALCcontext* context = alcCreateContext(device, NULL);
  if (!context || !alcMakeContextCurrent(context) || alcGetError(device) != ALC_NO_ERROR) {
    error("Unable to create OpenAL context");
  }

  state.device = device;
  state.context = context;
}

void lovrAudioDestroy() {
  alcMakeContextCurrent(NULL);
  alcDestroyContext(state.context);
  alcCloseDevice(state.device);
}
