#include "audio/audio.h"
#include "audio/source.h"
#include "data/audioStream.h"
#include "core/arr.h"
#include "core/maf.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

static struct {
  bool initialized;
  bool spatialized;
  ALCdevice* device;
  ALCcontext* context;
  float LOVR_ALIGN(16) orientation[4];
  float LOVR_ALIGN(16) position[4];
  float LOVR_ALIGN(16) velocity[4];
  arr_t(Source*) sources;
} state;

ALenum lovrAudioConvertFormat(uint32_t bitDepth, uint32_t channelCount) {
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

bool lovrAudioInit() {
  if (state.initialized) return false;

  ALCdevice* device = alcOpenDevice(NULL);
  lovrAssert(device, "Unable to open default audio device");

  ALCcontext* context = alcCreateContext(device, NULL);
  if (!context || !alcMakeContextCurrent(context) || alcGetError(device) != ALC_NO_ERROR) {
    lovrThrow("Unable to create OpenAL context");
  }

#if ALC_SOFT_HRTF
  static LPALCRESETDEVICESOFT alcResetDeviceSOFT;
  alcResetDeviceSOFT = (LPALCRESETDEVICESOFT) alcGetProcAddress(device, "alcResetDeviceSOFT");
  state.spatialized = alcIsExtensionPresent(device, "ALC_SOFT_HRTF");

  if (state.spatialized) {
    alcResetDeviceSOFT(device, (ALCint[]) { ALC_HRTF_SOFT, ALC_TRUE, 0 });
  }
#endif

  state.device = device;
  state.context = context;
  arr_init(&state.sources);
  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  alcMakeContextCurrent(NULL);
  alcDestroyContext(state.context);
  alcCloseDevice(state.device);
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrRelease(Source, state.sources.data[i]);
  }
  arr_free(&state.sources);
  memset(&state, 0, sizeof(state));
}

void lovrAudioUpdate() {
  for (size_t i = state.sources.length; i-- > 0;) {
    Source* source = state.sources.data[i];

    if (lovrSourceGetType(source) == SOURCE_STATIC) {
      continue;
    }

    uint32_t id = lovrSourceGetId(source);
    bool isStopped = lovrSourceIsStopped(source);
    ALint processed;
    alGetSourcei(id, AL_BUFFERS_PROCESSED, &processed);

    if (processed) {
      ALuint buffers[SOURCE_BUFFERS];
      alSourceUnqueueBuffers(id, processed, buffers);
      lovrSourceStream(source, buffers, processed);
      if (isStopped) {
        alSourcePlay(id);
      }
    } else if (isStopped) {
      // in case we'll play this source in the future, rewind it now. This also frees up queued raw buffers.
      lovrAudioStreamRewind(lovrSourceGetStream(source));

      arr_splice(&state.sources, i, 1);
      lovrRelease(Source, source);
    }
  }
}

void lovrAudioAdd(Source* source) {
  if (!lovrAudioHas(source)) {
    lovrRetain(source);
    arr_push(&state.sources, source);
  }
}

void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound) {
  alGetFloatv(AL_DOPPLER_FACTOR, factor);
  alGetFloatv(AL_SPEED_OF_SOUND, speedOfSound);
}

void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint32_t* count) {
  const char* name = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
  *count = 0;
  while (*name) {
    names[(*count)++] = name;
    name += strlen(name);
  }
}

void lovrAudioGetOrientation(quat orientation) {
  quat_init(orientation, state.orientation);
}

void lovrAudioGetPosition(vec3 position) {
  vec3_init(position, state.position);
}

void lovrAudioGetVelocity(vec3 velocity) {
  vec3_init(velocity, state.velocity);
}

float lovrAudioGetVolume() {
  float volume;
  alGetListenerf(AL_GAIN, &volume);
  return volume;
}

bool lovrAudioHas(Source* source) {
  for (size_t i = 0; i < state.sources.length; i++) {
    if (state.sources.data[i] == source) {
      return true;
    }
  }

  return false;
}

bool lovrAudioIsSpatialized() {
  return state.spatialized;
}

void lovrAudioPause() {
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrSourcePause(state.sources.data[i]);
  }
}

void lovrAudioResume() {
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrSourceResume(state.sources.data[i]);
  }
}

void lovrAudioRewind() {
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrSourceRewind(state.sources.data[i]);
  }
}

void lovrAudioSetDopplerEffect(float factor, float speedOfSound) {
  alDopplerFactor(factor);
  alSpeedOfSound(speedOfSound);
}

void lovrAudioSetOrientation(quat orientation) {

  // Rotate the unit forward/up vectors by the quaternion derived from the specified angle/axis
  float f[4] = { 0.f, 0.f, -1.f };
  float u[4] = { 0.f, 1.f,  0.f };
  quat_init(state.orientation, orientation);
  quat_rotate(state.orientation, f);
  quat_rotate(state.orientation, u);

  // Pass the rotated orientation vectors to OpenAL
  ALfloat directionVectors[6] = { f[0], f[1], f[2], u[0], u[1], u[2] };
  alListenerfv(AL_ORIENTATION, directionVectors);
}

void lovrAudioSetPosition(vec3 position) {
  vec3_init(state.position, position);
  alListenerfv(AL_POSITION, position);
}

void lovrAudioSetVelocity(vec3 velocity) {
  vec3_init(state.velocity, velocity);
  alListenerfv(AL_VELOCITY, velocity);
}

void lovrAudioSetVolume(float volume) {
  alListenerf(AL_GAIN, volume);
}

void lovrAudioStop() {
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrSourceStop(state.sources.data[i]);
  }
}
