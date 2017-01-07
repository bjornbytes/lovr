#include "audio/audio.h"
#include "loaders/source.h"
#include "util.h"
#include <stdlib.h>
#include <math.h>

static AudioState state;

static LPALCRESETDEVICESOFT alcResetDeviceSOFT;

static void cross(float ux, float uy, float uz, float vx, float vy, float vz, float* x, float* y, float* z) {
  *x = uy * vz - uz * vy;
  *y = ux * vz - uz * vx;
  *z = ux * vy - uy * vx;
}

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

void lovrAudioGetOrientation(float* angle, float* ax, float* ay, float* az) {
  float v[6];
  alGetListenerfv(AL_ORIENTATION, v);
  float cx, cy, cz;
  cross(v[0], v[1], v[2], v[3], v[4], v[5], &cx, &cy, &cz);
  float w = 1 + v[0] * v[3] + v[1] * v[4] + v[2] * v[5];
  *angle = 2 * acos(w);
  float s = sqrt(1 - w * w);
  if (w < .001) {
    *ax = cx;
    *ay = cy;
    *az = cz;
  } else {
    *ax = cx / s;
    *ay = cy / s;
    *az = cz / s;
  }
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

// Help
void lovrAudioSetOrientation(float angle, float ax, float ay, float az) {

  // Quaternion
  float cos2 = cos(angle / 2.f);
  float sin2 = sin(angle / 2.f);
  float qx = sin2 * ax;
  float qy = sin2 * ay;
  float qz = sin2 * az;
  float s = cos2;

  float vx, vy, vz, qdotv, qdotq, a, b, c, cx, cy, cz;

  // Forward
  vx = 0;
  vy = 0;
  vz = -1;
  qdotv = qx * vx + qy * vy + qz * vz;
  qdotq = qx * qx + qy * qy + qz * qz;
  a = 2 * qdotv;
  b = s * s - qdotq;
  c = 2 * s;
  cross(qx, qy, qz, vx, vy, vz, &cx, &cy, &cz);
  float fx = a * qx + b * vx + c * cx;
  float fy = a * qy + b * vy + c * cy;
  float fz = a * qz + b * vz + c * cz;

  // Up
  vx = 0;
  vy = 1;
  vz = 0;
  qdotv = qx * vx + qy * vy + qz * vz;
  qdotq = qx * qx + qy * qy + qz * qz;
  a = 2 * qdotv;
  b = s * s - qdotq;
  c = 2 * s;
  cross(qx, qy, qz, vx, vy, vz, &cx, &cy, &cz);
  float ux = a * qx + b * vx + c * cx;
  float uy = a * qy + b * vy + c * cy;
  float uz = a * qz + b * vz + c * cz;

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
