#include "audio/audio.h"
#include "loaders/source.h"
#include "util.h"
#include <stdlib.h>

static AudioState state;

static LPALCRESETDEVICESOFT alcResetDeviceSOFT;

void lovrAudioInit() {
  ALCdevice* device = alcOpenDevice(NULL);
  if (!device) {
    error("Unable to open default audio device");
  }

  ALCcontext* context = alcCreateContext(device, NULL);
  if (!context || !alcMakeContextCurrent(context) || alcGetError(device) != ALC_NO_ERROR) {
    error("Unable to create OpenAL context");
  }

  alcResetDeviceSOFT = (LPALCRESETDEVICESOFT) alcGetProcAddress(device, "alcResetDeviceSOFT");

  if (alcIsExtensionPresent(device, "ALC_SOFT_HRTF")) {
    ALCint attrs[3] = { ALC_HRTF_SOFT, ALC_TRUE, 0 };
    alcResetDeviceSOFT(device, attrs);
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
    int isStopped = lovrSourceIsStopped(source);
    ALint processed;
    alGetSourcei(source->id, AL_BUFFERS_PROCESSED, &processed);

    if (processed) {
      ALuint buffers[SOURCE_BUFFERS];
      alSourceUnqueueBuffers(source->id, processed, buffers);
      lovrSourceStream(source, buffers, processed);
      if (isStopped) {
        alSourcePlay(source->id);
      }
    } else if (isStopped) {
      vec_splice(&state.sources, i, 1);
      lovrRelease(&source->ref);
    }
  }
}

void lovrAudioAdd(Source* source) {
  if (!lovrAudioHas(source)) {
    lovrRetain(&source->ref);
    vec_push(&state.sources, source);
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

float lovrAudioGetVolume() {
  float volume;
  alGetListenerf(AL_GAIN, &volume);
  return volume;
}

int lovrAudioHas(Source* source) {
  int index;
  vec_find(&state.sources, source, index);
  return index >= 0;
}

void lovrAudioPause() {
  int i; Source* source;
  vec_foreach(&state.sources, source, i) {
    lovrSourcePause(source);
  }
}

void lovrAudioResume() {
  int i; Source* source;
  vec_foreach(&state.sources, source, i) {
    lovrSourceResume(source);
  }
}

void lovrAudioRewind() {
  int i; Source* source;
  vec_foreach(&state.sources, source, i) {
    lovrSourceRewind(source);
  }
}

void lovrAudioSetOrientation(float fx, float fy, float fz, float ux, float uy, float uz) {
  ALfloat orientation[6] = { fx, fy, fz, ux, uy, uz };
  alListenerfv(AL_ORIENTATION, orientation);
}

void lovrAudioSetPosition(float x, float y, float z) {
  alListener3f(AL_POSITION, x, y, z);
}

void lovrAudioSetVolume(float volume) {
  alListenerf(AL_GAIN, volume);
}

void lovrAudioStop() {
  int i; Source* source;
  vec_foreach(&state.sources, source, i) {
    lovrSourceStop(source);
  }
}
