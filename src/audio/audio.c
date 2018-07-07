#include "audio/audio.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "util.h"
#include <stdlib.h>

static AudioState state;

ALenum lovrAudioConvertFormat(int bitDepth, int channelCount) {
  if (bitDepth == 8 && channelCount == 1) {
    return AL_FORMAT_MONO8;
  } else if (bitDepth == 8 && channelCount == 2) {
    return AL_FORMAT_STEREO8;
  } else if (bitDepth == 16 && channelCount == 1) {
    return AL_FORMAT_MONO16;
  } else if (bitDepth == 16 && channelCount == 2) {
    return AL_FORMAT_STEREO16;
  }

  return 0;
}

void lovrAudioInit() {
  if (state.initialized) return;

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
    alcResetDeviceSOFT(device, (ALCint[]) { ALC_HRTF_SOFT, ALC_TRUE, 0 });
  }

  state.device = device;
  state.context = context;
  vec_init(&state.sources);
  quat_set(state.orientation, 0, 0, 0, -1);
  vec3_set(state.position, 0, 0, 0);
  vec3_set(state.velocity, 0, 0, 0);
  state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  alcMakeContextCurrent(NULL);
  alcDestroyContext(state.context);
  alcCloseDevice(state.device);
  for (int i = 0; i < state.sources.length; i++) {
    lovrRelease(state.sources.data[i]);
  }
  vec_deinit(&state.sources);
  memset(&state, 0, sizeof(AudioState));
}

void lovrAudioUpdate() {
  int i; Source* source;
  vec_foreach_rev(&state.sources, source, i) {
    if (source->type == SOURCE_STATIC) {
      continue;
    }

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
      lovrAudioStreamRewind(source->stream);
      vec_splice(&state.sources, i, 1);
      lovrRelease(source);
    }
  }
}

void lovrAudioAdd(Source* source) {
  if (!lovrAudioHas(source)) {
    lovrRetain(source);
    vec_push(&state.sources, source);
  }
}

void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound) {
  alGetFloatv(AL_DOPPLER_FACTOR, factor);
  alGetFloatv(AL_SPEED_OF_SOUND, speedOfSound);
}

void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint8_t* count) {
  const char* name = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
  *count = 0;
  while (*name) {
    names[(*count)++] = name;
    name += strlen(name);
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

void lovrAudioSetDopplerEffect(float factor, float speedOfSound) {
  alDopplerFactor(factor);
  alSpeedOfSound(speedOfSound);
}

void lovrAudioSetOrientation(float angle, float ax, float ay, float az) {

  // Rotate the unit forward/up vectors by the quaternion derived from the specified angle/axis
  float f[3] = { 0, 0, -1 };
  float u[3] = { 0, 1, 0 };
  quat_fromAngleAxis(state.orientation, angle, (float[3]) { ax, ay, az });
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
