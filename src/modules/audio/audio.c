#include "audio/audio.h"
#include "data/audioStream.h"
#include "data/soundData.h"
#include "core/arr.h"
#include "core/maf.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <AL/al.h>
#include <AL/alc.h>
#ifndef EMSCRIPTEN
#include <AL/alext.h>
#endif

#define SOURCE_BUFFERS 4

struct Source {
  SourceType type;
  struct SoundData* soundData;
  struct AudioStream* stream;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  bool isLooping;
};

struct Microphone {
  ALCdevice* device;
  const char* name;
  bool isRecording;
  uint32_t sampleRate;
  uint32_t bitDepth;
  uint32_t channelCount;
};

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

static ALenum lovrAudioConvertFormat(uint32_t bitDepth, uint32_t channelCount) {
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
  for (size_t i = 0; i < state.sources.length; i++) {
    lovrRelease(Source, state.sources.data[i]);
  }
  arr_free(&state.sources);
  alcMakeContextCurrent(NULL);
  alcDestroyContext(state.context);
  alcCloseDevice(state.device);
  memset(&state, 0, sizeof(state));
}

void lovrAudioUpdate() {
  for (size_t i = state.sources.length; i-- > 0;) {
    Source* source = state.sources.data[i];

    if (lovrSourceGetType(source) == SOURCE_STATIC) {
      continue;
    }

    ALenum sourceState;
    alGetSourcei(source->id, AL_SOURCE_STATE, &sourceState);
    bool isStopped = sourceState == AL_STOPPED;
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
      // in case we'll play this source in the future, rewind it now. This also frees up queued raw buffers.
      lovrAudioStreamRewind(source->stream);

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

// Source

Source* lovrSourceCreateStatic(SoundData* soundData) {
  Source* source = lovrAlloc(Source);
  ALenum format = lovrAudioConvertFormat(soundData->bitDepth, soundData->channelCount);
  source->type = SOURCE_STATIC;
  source->soundData = soundData;
  alGenSources(1, &source->id);
  alGenBuffers(1, source->buffers);
  alBufferData(source->buffers[0], format, soundData->blob->data, (ALsizei) soundData->blob->size, soundData->sampleRate);
  alSourcei(source->id, AL_BUFFER, source->buffers[0]);
  lovrRetain(soundData);
  return source;
}

Source* lovrSourceCreateStream(AudioStream* stream) {
  Source* source = lovrAlloc(Source);
  source->type = SOURCE_STREAM;
  source->stream = stream;
  alGenSources(1, &source->id);
  alGenBuffers(SOURCE_BUFFERS, source->buffers);
  lovrRetain(stream);
  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  alDeleteSources(1, &source->id);
  alDeleteBuffers(source->type == SOURCE_STATIC ? 1 : SOURCE_BUFFERS, source->buffers);
  lovrRelease(SoundData, source->soundData);
  lovrRelease(AudioStream, source->stream);
}

SourceType lovrSourceGetType(Source* source) {
  return source->type;
}

uint32_t lovrSourceGetBitDepth(Source* source) {
  return source->type == SOURCE_STATIC ? source->soundData->bitDepth : source->stream->bitDepth;
}

void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain) {
  alGetSourcef(source->id, AL_CONE_INNER_ANGLE, innerAngle);
  alGetSourcef(source->id, AL_CONE_OUTER_ANGLE, outerAngle);
  alGetSourcef(source->id, AL_CONE_OUTER_GAIN, outerGain);
  *innerAngle *= (float) M_PI / 180.f;
  *outerAngle *= (float) M_PI / 180.f;
}

uint32_t lovrSourceGetChannelCount(Source* source) {
  return source->type == SOURCE_STATIC ? source->soundData->channelCount : source->stream->channelCount;
}

void lovrSourceGetOrientation(Source* source, quat orientation) {
  float v[4], forward[4] = { 0.f, 0.f, -1.f };
  alGetSourcefv(source->id, AL_DIRECTION, v);
  quat_between(orientation, forward, v);
}

size_t lovrSourceGetDuration(Source* source) {
  return source->type == SOURCE_STATIC ? source->soundData->samples : source->stream->samples;
}

void lovrSourceGetFalloff(Source* source, float* reference, float* max, float* rolloff) {
  alGetSourcef(source->id, AL_REFERENCE_DISTANCE, reference);
  alGetSourcef(source->id, AL_MAX_DISTANCE, max);
  alGetSourcef(source->id, AL_ROLLOFF_FACTOR, rolloff);
}

float lovrSourceGetPitch(Source* source) {
  float pitch;
  alGetSourcef(source->id, AL_PITCH, &pitch);
  return pitch;
}

void lovrSourceGetPosition(Source* source, vec3 position) {
  alGetSourcefv(source->id, AL_POSITION, position);
}

uint32_t lovrSourceGetSampleRate(Source* source) {
  return source->type == SOURCE_STATIC ? source->soundData->sampleRate : source->stream->sampleRate;
}

void lovrSourceGetVelocity(Source* source, vec3 velocity) {
  alGetSourcefv(source->id, AL_VELOCITY, velocity);
}

float lovrSourceGetVolume(Source* source) {
  float volume;
  alGetSourcef(source->id, AL_GAIN, &volume);
  return volume;
}

void lovrSourceGetVolumeLimits(Source* source, float* min, float* max) {
  alGetSourcef(source->id, AL_MIN_GAIN, min);
  alGetSourcef(source->id, AL_MAX_GAIN, max);
}

bool lovrSourceIsLooping(Source* source) {
  return source->isLooping;
}

bool lovrSourceIsPlaying(Source* source) {
  ALenum state;
  alGetSourcei(source->id, AL_SOURCE_STATE, &state);
  return state == AL_PLAYING;
}

bool lovrSourceIsRelative(Source* source) {
  int isRelative;
  alGetSourcei(source->id, AL_SOURCE_RELATIVE, &isRelative);
  return isRelative == AL_TRUE;
}

void lovrSourcePause(Source* source) {
  alSourcePause(source->id);
}

void lovrSourcePlay(Source* source) {
  ALenum state;
  alGetSourcei(source->id, AL_SOURCE_STATE, &state);

  if (source->type == SOURCE_STATIC) {
    if (state != AL_PLAYING) {
      alSourcePlay(source->id);
    }
  } else {
    switch (state) {
      case AL_INITIAL:
      case AL_STOPPED:
        alSourcei(source->id, AL_BUFFER, AL_NONE);
        lovrSourceStream(source, source->buffers, SOURCE_BUFFERS);
        alSourcePlay(source->id);
        break;
      case AL_PAUSED:
        alSourcePlay(source->id);
        break;
      case AL_PLAYING:
        break;
    }
  }
}

void lovrSourceSeek(Source* source, size_t sample) {
  if (source->type == SOURCE_STATIC) {
    alSourcef(source->id, AL_SAMPLE_OFFSET, sample);
  } else {
    ALenum state;
    alGetSourcei(source->id, AL_SOURCE_STATE, &state);
    bool wasPaused = state == AL_PAUSED;
    alSourceStop(source->id);
    lovrAudioStreamSeek(source->stream, sample);
    lovrSourcePlay(source);
    if (wasPaused) {
      lovrSourcePause(source);
    }
  }
}

void lovrSourceSetCone(Source* source, float innerAngle, float outerAngle, float outerGain) {
  alSourcef(source->id, AL_CONE_INNER_ANGLE, innerAngle * 180.f / (float) M_PI);
  alSourcef(source->id, AL_CONE_OUTER_ANGLE, outerAngle * 180.f / (float) M_PI);
  alSourcef(source->id, AL_CONE_OUTER_GAIN, outerGain);
}

void lovrSourceSetOrientation(Source* source, quat orientation) {
  float v[4] = { 0.f, 0.f, -1.f };
  quat_rotate(orientation, v);
  alSource3f(source->id, AL_DIRECTION, v[0], v[1], v[2]);
}

void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff) {
  lovrAssert(lovrSourceGetChannelCount(source) == 1, "Positional audio is only supported for mono sources");
  alSourcef(source->id, AL_REFERENCE_DISTANCE, reference);
  alSourcef(source->id, AL_MAX_DISTANCE, max);
  alSourcef(source->id, AL_ROLLOFF_FACTOR, rolloff);
}

void lovrSourceSetLooping(Source* source, bool isLooping) {
  lovrAssert(!source->stream || !lovrAudioStreamIsRaw(source->stream), "Can't loop a raw stream");
  source->isLooping = isLooping;
  if (source->type == SOURCE_STATIC) {
    alSourcei(source->id, AL_LOOPING, isLooping ? AL_TRUE : AL_FALSE);
  }
}

void lovrSourceSetPitch(Source* source, float pitch) {
  alSourcef(source->id, AL_PITCH, pitch);
}

void lovrSourceSetPosition(Source* source, vec3 position) {
  lovrAssert(lovrSourceGetChannelCount(source) == 1, "Positional audio is only supported for mono sources");
  alSource3f(source->id, AL_POSITION, position[0], position[1], position[2]);
}

void lovrSourceSetRelative(Source* source, bool isRelative) {
  alSourcei(source->id, AL_SOURCE_RELATIVE, isRelative ? AL_TRUE : AL_FALSE);
}

void lovrSourceSetVelocity(Source* source, vec3 velocity) {
  alSource3f(source->id, AL_VELOCITY, velocity[0], velocity[1], velocity[2]);
}

void lovrSourceSetVolume(Source* source, float volume) {
  alSourcef(source->id, AL_GAIN, volume);
}

void lovrSourceSetVolumeLimits(Source* source, float min, float max) {
  alSourcef(source->id, AL_MIN_GAIN, min);
  alSourcef(source->id, AL_MAX_GAIN, max);
}

void lovrSourceStop(Source* source) {
  if (source->type == SOURCE_STATIC) {
    alSourceStop(source->id);
  } else {
    alSourceStop(source->id);
    alSourcei(source->id, AL_BUFFER, AL_NONE);
    lovrAudioStreamRewind(source->stream);
  }
}

// Fills buffers with data and queues them, called once initially and over time to stream more data
void lovrSourceStream(Source* source, ALuint* buffers, size_t count) {
  if (source->type == SOURCE_STATIC) {
    return;
  }

  AudioStream* stream = source->stream;
  ALenum format = lovrAudioConvertFormat(stream->bitDepth, stream->channelCount);
  uint32_t frequency = stream->sampleRate;
  size_t samples = 0;
  size_t n = 0;

  // Keep decoding until there is nothing left to decode or all the buffers are filled
  while (n < count && (samples = lovrAudioStreamDecode(stream, NULL, 0)) != 0) {
    alBufferData(buffers[n++], format, stream->buffer, (ALsizei) (samples * sizeof(ALshort)), frequency);
  }

  alSourceQueueBuffers(source->id, (ALsizei) n, buffers);

  if (samples == 0 && source->isLooping && n < count) {
    lovrAudioStreamRewind(stream);
    lovrSourceStream(source, buffers + n, count - n);
    return;
  }
}

size_t lovrSourceTell(Source* source) {
  switch (source->type) {
    case SOURCE_STATIC: {
      float offset;
      alGetSourcef(source->id, AL_SAMPLE_OFFSET, &offset);
      return offset;
    }

    case SOURCE_STREAM: {
      size_t decoderOffset = lovrAudioStreamTell(source->stream);
      size_t samplesPerBuffer = source->stream->bufferSize / source->stream->channelCount / sizeof(ALshort);
      ALsizei queuedBuffers, sampleOffset;
      alGetSourcei(source->id, AL_BUFFERS_QUEUED, &queuedBuffers);
      alGetSourcei(source->id, AL_SAMPLE_OFFSET, &sampleOffset);

      size_t offset = decoderOffset + sampleOffset;

      if (queuedBuffers * samplesPerBuffer > offset) {
        return offset + source->stream->samples;
      } else {
        return offset;
      }
      break;
    }

    default: lovrThrow("Unreachable"); break;
  }
}

// Microphone

Microphone* lovrMicrophoneCreate(const char* name, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount) {
  Microphone* microphone = lovrAlloc(Microphone);
  ALCdevice* device = alcCaptureOpenDevice(name, sampleRate, lovrAudioConvertFormat(bitDepth, channelCount), (ALCsizei) samples);
  lovrAssert(device, "Error opening capture device for microphone '%s'", name);
  microphone->device = device;
  microphone->name = name ? name : alcGetString(device, ALC_CAPTURE_DEVICE_SPECIFIER);
  microphone->sampleRate = sampleRate;
  microphone->bitDepth = bitDepth;
  microphone->channelCount = channelCount;
  return microphone;
}

void lovrMicrophoneDestroy(void* ref) {
  Microphone* microphone = ref;
  lovrMicrophoneStopRecording(microphone);
  alcCaptureCloseDevice(microphone->device);
}

uint32_t lovrMicrophoneGetBitDepth(Microphone* microphone) {
  return microphone->bitDepth;
}

uint32_t lovrMicrophoneGetChannelCount(Microphone* microphone) {
  return microphone->channelCount;
}

SoundData* lovrMicrophoneGetData(Microphone* microphone, size_t samples, SoundData* soundData, size_t offset) {
  size_t availableSamples = lovrMicrophoneGetSampleCount(microphone);

  if (!microphone->isRecording || availableSamples == 0) {
    return NULL;
  }
  
  if (samples == 0 || samples > availableSamples) {
    samples = availableSamples;
  }

  if (soundData == NULL) {
    soundData = lovrSoundDataCreate(samples, microphone->sampleRate, microphone->bitDepth, microphone->channelCount);
  } else {
    lovrAssert(soundData->channelCount == microphone->channelCount, "Microphone and SoundData channel counts must match");
    lovrAssert(soundData->sampleRate == microphone->sampleRate, "Microphone and SoundData sample rates must match");
    lovrAssert(soundData->bitDepth == microphone->bitDepth, "Microphone and SoundData bit depths must match");
    lovrAssert(offset + samples <= soundData->samples, "Tried to write samples past the end of a SoundData buffer");
  }

  uint8_t* data = (uint8_t*) soundData->blob->data + offset * (microphone->bitDepth / 8) * microphone->channelCount;
  alcCaptureSamples(microphone->device, data, (ALCsizei) samples);
  return soundData;
}

const char* lovrMicrophoneGetName(Microphone* microphone) {
  return microphone->name;
}

size_t lovrMicrophoneGetSampleCount(Microphone* microphone) {
  if (!microphone->isRecording) {
    return 0;
  }

  ALCint samples;
  alcGetIntegerv(microphone->device, ALC_CAPTURE_SAMPLES, sizeof(ALCint), &samples);
  return (size_t) samples;
}

uint32_t lovrMicrophoneGetSampleRate(Microphone* microphone) {
  return microphone->sampleRate;
}

bool lovrMicrophoneIsRecording(Microphone* microphone) {
  return microphone->isRecording;
}

void lovrMicrophoneStartRecording(Microphone* microphone) {
  if (microphone->isRecording) {
    return;
  }

  alcCaptureStart(microphone->device);
  microphone->isRecording = true;
}

void lovrMicrophoneStopRecording(Microphone* microphone) {
  if (!microphone->isRecording) {
    return;
  }

  alcCaptureStop(microphone->device);
  microphone->isRecording = false;
}
