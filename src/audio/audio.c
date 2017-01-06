#include "audio/audio.h"
#include "loaders/source.h"
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
  vec_init(&state.sources);
}

void lovrAudioDestroy() {
  alcMakeContextCurrent(NULL);
  alcDestroyContext(state.context);
  alcCloseDevice(state.device);
  vec_deinit(&state.sources);
}

void lovrAudioUpdate() {
  int i; Source* source;
  vec_foreach_rev(&state.sources, source, i) {
    if (lovrSourceIsStopped(source)) {
      vec_splice(&state.sources, i, 1);
      lovrRelease(&source->ref);
      continue;
    }

    ALint count;
    alGetSourcei(source->id, AL_BUFFERS_PROCESSED, &count);
    if (count > 0) {
      ALuint buffers[SOURCE_BUFFERS];
      alSourceUnqueueBuffers(source->id, count, buffers);
      lovrSourceStream(source, buffers, count);
    }
  }
}

void lovrAudioPlay(Source* source) {
  lovrRetain(&source->ref);
  vec_push(&state.sources, source);
}
