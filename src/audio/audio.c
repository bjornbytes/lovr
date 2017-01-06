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
    if (lovrSourceIsStopped(source) && !source->isLooping) {
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

void lovrAudioGetOrientation(float* fx, float* fy, float* fz, float* ux, float* uy, float* uz) {
  float v[6];
  alGetListenerfv(AL_ORIENTATION, v);
  *fx = v[0];
  *fy = v[1];
  *fz = v[2];
  *ux = v[3];
  *uy = v[4];
  *uz = v[5];
}

void lovrAudioGetPosition(float* x, float* y, float* z) {
  alGetListener3f(AL_POSITION, x, y, z);
}

void lovrAudioPlay(Source* source) {
  lovrRetain(&source->ref);
  vec_push(&state.sources, source);
}

void lovrAudioSetOrientation(float fx, float fy, float fz, float ux, float uy, float uz) {
  ALfloat orientation[6] = { fx, fy, fz, ux, uy, uz };
  alListenerfv(AL_ORIENTATION, orientation);
}

void lovrAudioSetPosition(float x, float y, float z) {
  alListener3f(AL_POSITION, x, y, z);
}
