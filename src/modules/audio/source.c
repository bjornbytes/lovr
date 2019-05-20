#include "audio/source.h"
#include "audio/audio.h"
#include "data/audioStream.h"
#include "data/soundData.h"
#include "core/maf.h"
#include "types.h"
#include <math.h>
#include <stdlib.h>
#include <AL/al.h>
#include <AL/alc.h>

#define SOURCE_BUFFERS 4

struct Source {
  Ref ref;
  SourceType type;
  struct SoundData* soundData;
  struct AudioStream* stream;
  ALuint id;
  ALuint buffers[SOURCE_BUFFERS];
  bool isLooping;
};

static ALenum lovrSourceGetState(Source* source) {
  ALenum state;
  alGetSourcei(source->id, AL_SOURCE_STATE, &state);
  return state;
}

Source* lovrSourceCreateStatic(SoundData* soundData) {
  Source* source = lovrAlloc(Source);
  ALenum format = lovrAudioConvertFormat(soundData->bitDepth, soundData->channelCount);
  source->type = SOURCE_STATIC;
  source->soundData = soundData;
  alGenSources(1, &source->id);
  alGenBuffers(1, source->buffers);
  alBufferData(source->buffers[0], format, soundData->blob.data, (ALsizei) soundData->blob.size, soundData->sampleRate);
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

uint32_t lovrSourceGetId(Source* source) {
  return source->id;
}

AudioStream* lovrSourceGetStream(Source* source) {
  return source->stream;
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
  float v[3], forward[3] = { 0.f, 0.f, -1.f };
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

bool lovrSourceIsPaused(Source* source) {
  return lovrSourceGetState(source) == AL_PAUSED;
}

bool lovrSourceIsPlaying(Source* source) {
  return lovrSourceGetState(source) == AL_PLAYING;
}

bool lovrSourceIsRelative(Source* source) {
  int isRelative;
  alGetSourcei(source->id, AL_SOURCE_RELATIVE, &isRelative);
  return isRelative == AL_TRUE;
}

bool lovrSourceIsStopped(Source* source) {
  return lovrSourceGetState(source) == AL_STOPPED;
}

void lovrSourcePause(Source* source) {
  alSourcePause(source->id);
}

void lovrSourcePlay(Source* source) {
  if (lovrSourceIsPlaying(source)) {
    return;
  } else if (lovrSourceIsPaused(source)) {
    lovrSourceResume(source);
    return;
  }

  lovrSourceStream(source, source->buffers, SOURCE_BUFFERS);
  alSourcePlay(source->id);
}

void lovrSourceResume(Source* source) {
  if (!lovrSourceIsPaused(source)) {
    return;
  }

  alSourcePlay(source->id);
}

void lovrSourceRewind(Source* source) {
  if (lovrSourceIsStopped(source)) {
    return;
  }

  bool wasPaused = lovrSourceIsPaused(source);
  alSourceRewind(source->id);
  lovrSourceStop(source);
  lovrSourcePlay(source);
  if (wasPaused) {
    lovrSourcePause(source);
  }
}

void lovrSourceSeek(Source* source, size_t sample) {
  switch (source->type) {
    case SOURCE_STATIC:
      alSourcef(source->id, AL_SAMPLE_OFFSET, sample);
      break;

    case SOURCE_STREAM: {
      bool wasPaused = lovrSourceIsPaused(source);
      lovrSourceStop(source);
      lovrAudioStreamSeek(source->stream, sample);
      lovrSourcePlay(source);
      if (wasPaused) {
        lovrSourcePause(source);
      }
      break;
    }
  }
}

void lovrSourceSetCone(Source* source, float innerAngle, float outerAngle, float outerGain) {
  alSourcef(source->id, AL_CONE_INNER_ANGLE, innerAngle * 180.f / (float) M_PI);
  alSourcef(source->id, AL_CONE_OUTER_ANGLE, outerAngle * 180.f / (float) M_PI);
  alSourcef(source->id, AL_CONE_OUTER_GAIN, outerGain);
}

void lovrSourceSetOrientation(Source* source, quat orientation) {
  float v[3] = { 0.f, 0.f, -1.f };
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
  if (lovrSourceIsStopped(source)) {
    return;
  }

  switch (source->type) {
    case SOURCE_STATIC:
      alSourceStop(source->id);
      break;

    case SOURCE_STREAM: {

      // Empty the buffers
      int count = 0;
      alGetSourcei(source->id, AL_BUFFERS_QUEUED, &count);
      alSourceUnqueueBuffers(source->id, count, NULL);

      // Stop the source
      alSourceStop(source->id);
      alSourcei(source->id, AL_BUFFER, AL_NONE);

      // Rewind the decoder
      lovrAudioStreamRewind(source->stream);
      break;
    }
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
    alBufferData(buffers[n++], format, stream->buffer, samples * sizeof(ALshort), frequency);
  }

  alSourceQueueBuffers(source->id, n, buffers);

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
