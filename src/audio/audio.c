#include "audio/audio.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "util.h"
#include <stdlib.h>
#include "lovr.h"

static AudioState state;
static bool audioAlreadyInit = false;

void lovrAudioInit() {
  if (audioAlreadyInit) { // During a reload, bring down the audio device then recreate it
    lovrAudioDestroy();
  }

  ALCdevice* device = alcOpenDevice(NULL);
  lovrAssert(device, "Unable to open default audio device");

  ALCcontext* context = alcCreateContext(device, NULL);
  if (!context || !alcMakeContextCurrent(context) || alcGetError(device) != ALC_NO_ERROR) {
    lovrThrow("Unable to create OpenAL context");
  }

  static LPALCRESETDEVICESOFT alcResetDeviceSOFT;
  alcResetDeviceSOFT = (LPALCRESETDEVICESOFT) alcGetProcAddress(device, "alcResetDeviceSOFT");
  state.isSpatialized = alcIsExtensionPresent(device, "ALC_SOFT_HRTF");

  if (state.isSpatialized) {
    ALCint attrs[3] = { ALC_HRTF_SOFT, ALC_TRUE, 0 };
    alcResetDeviceSOFT(device, attrs);
  }

  state.device = device;
  state.context = context;
  vec_init(&state.sources);
  quat_set(state.orientation, 0, 0, 0, -1);
  vec3_set(state.position, 0, 0, 0);
  vec3_set(state.velocity, 0, 0, 0);

  if (!audioAlreadyInit) {
    atexit(lovrAudioDestroy);
    audioAlreadyInit = true;
  }
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
    bool isStopped = lovrSourceIsStopped(source);
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

void lovrAudioGetOrientation(float* angle, float* ax, float* ay, float* az) {
  quat_getAngleAxis(state.orientation, angle, ax, ay, az);
}

void lovrAudioGetPosition(float* x, float* y, float* z) {
  *x = state.position[0];
  *y = state.position[1];
  *z = state.position[2];
}

void lovrAudioGetVelocity(float* x, float* y, float* z) {
  *x = state.velocity[0];
  *y = state.velocity[1];
  *z = state.velocity[2];
}

float lovrAudioGetVolume() {
  float volume;
  alGetListenerf(AL_GAIN, &volume);
  return volume;
}

bool lovrAudioHas(Source* source) {
  int index;
  vec_find(&state.sources, source, index);
  return index >= 0;
}

bool lovrAudioIsSpatialized() {
  return state.isSpatialized;
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

void lovrAudioSetOrientation(float angle, float ax, float ay, float az) {

  // Rotate the unit forward/up vectors by the quaternion derived from the specified angle/axis
  float f[3] = { 0, 0, -1 };
  float u[3] = { 0, 1, 0 };
  float axis[3] = { ax, ay, az };
  quat_fromAngleAxis(state.orientation, angle, axis);
  quat_rotate(state.orientation, f);
  quat_rotate(state.orientation, u);

  // Pass the rotated orientation vectors to OpenAL
  ALfloat orientation[6] = { f[0], f[1], f[2], u[0], u[1], u[2] };
  alListenerfv(AL_ORIENTATION, orientation);
}

void lovrAudioSetPosition(float x, float y, float z) {
  vec3_set(state.position, x, y, z);
  alListener3f(AL_POSITION, x, y, z);
}

void lovrAudioSetVelocity(float x, float y, float z) {
  vec3_set(state.velocity, x, y, z);
  alListener3f(AL_VELOCITY, x, y, z);
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
