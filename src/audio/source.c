#include "audio/source.h"
#include "data/audioStream.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

static ALenum lovrSourceGetFormat(Source* source) {
  int channelCount = source->stream->channelCount;
  int bitDepth = source->stream->bitDepth;

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

static ALenum lovrSourceGetState(Source* source) {
  ALenum state;
  alGetSourcei(source->id, AL_SOURCE_STATE, &state);
  return state;
}

Source* lovrSourceCreate(AudioStream* stream) {
  Source* source = lovrAlloc(sizeof(Source), lovrSourceDestroy);
  if (!source) return NULL;

  source->stream = stream;
  source->isLooping = false;
  alGenSources(1, &source->id);
  alGenBuffers(SOURCE_BUFFERS, source->buffers);
  lovrRetain(&stream->ref);

  return source;
}

void lovrSourceDestroy(const Ref* ref) {
  Source* source = containerof(ref, Source);
  alDeleteSources(1, &source->id);
  alDeleteBuffers(SOURCE_BUFFERS, source->buffers);
  lovrRelease(&source->stream->ref);
  free(source);
}

int lovrSourceGetBitDepth(Source* source) {
  return source->stream->bitDepth;
}

void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain) {
  alGetSourcef(source->id, AL_CONE_INNER_ANGLE, innerAngle);
  alGetSourcef(source->id, AL_CONE_OUTER_ANGLE, outerAngle);
  alGetSourcef(source->id, AL_CONE_OUTER_GAIN, outerGain);
  *innerAngle *= M_PI / 180.f;
  *outerAngle *= M_PI / 180.f;
}

int lovrSourceGetChannelCount(Source* source) {
  return source->stream->channelCount;
}

void lovrSourceGetDirection(Source* source, float* x, float* y, float* z) {
  float v[3];
  alGetSourcefv(source->id, AL_DIRECTION, v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

int lovrSourceGetDuration(Source* source) {
  return source->stream->samples;
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

void lovrSourceGetPosition(Source* source, float* x, float* y, float* z) {
  float v[3];
  alGetSourcefv(source->id, AL_POSITION, v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
}

int lovrSourceGetSampleRate(Source* source) {
  return source->stream->sampleRate;
}

void lovrSourceGetVelocity(Source* source, float* x, float* y, float* z) {
  float v[3];
  alGetSourcefv(source->id, AL_VELOCITY, v);
  *x = v[0];
  *y = v[1];
  *z = v[2];
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

void lovrSourceSeek(Source* source, int sample) {
  bool wasPaused = lovrSourceIsPaused(source);
  lovrSourceStop(source);
  lovrAudioStreamSeek(source->stream, sample);
  lovrSourcePlay(source);
  if (wasPaused) {
    lovrSourcePause(source);
  }
}

void lovrSourceSetCone(Source* source, float innerAngle, float outerAngle, float outerGain) {
  alSourcef(source->id, AL_CONE_INNER_ANGLE, innerAngle * 180.f / M_PI);
  alSourcef(source->id, AL_CONE_OUTER_ANGLE, outerAngle * 180.f / M_PI);
  alSourcef(source->id, AL_CONE_OUTER_GAIN, outerGain);
}

void lovrSourceSetDirection(Source* source, float x, float y, float z) {
  alSource3f(source->id, AL_DIRECTION, x, y, z);
}

void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff) {
  lovrAssert(lovrSourceGetChannelCount(source) == 1, "Positional audio is only supported for mono sources");
  alSourcef(source->id, AL_REFERENCE_DISTANCE, reference);
  alSourcef(source->id, AL_MAX_DISTANCE, max);
  alSourcef(source->id, AL_ROLLOFF_FACTOR, rolloff);
}

void lovrSourceSetLooping(Source* source, bool isLooping) {
  source->isLooping = isLooping;
}

void lovrSourceSetPitch(Source* source, float pitch) {
  alSourcef(source->id, AL_PITCH, pitch);
}

void lovrSourceSetPosition(Source* source, float x, float y, float z) {
  lovrAssert(lovrSourceGetChannelCount(source) == 1, "Positional audio is only supported for mono sources");
  alSource3f(source->id, AL_POSITION, x, y, z);
}

void lovrSourceSetRelative(Source* source, bool isRelative) {
  alSourcei(source->id, AL_SOURCE_RELATIVE, isRelative ? AL_TRUE : AL_FALSE);
}

void lovrSourceSetVelocity(Source* source, float x, float y, float z) {
  alSource3f(source->id, AL_VELOCITY, x, y, z);
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

  // Empty the buffers
  int count = 0;
  alGetSourcei(source->id, AL_BUFFERS_QUEUED, &count);
  alSourceUnqueueBuffers(source->id, count, NULL);

  // Stop the source
  alSourceStop(source->id);
  alSourcei(source->id, AL_BUFFER, AL_NONE);

  // Rewind the decoder
  lovrAudioStreamRewind(source->stream);
}

// Fills buffers with data and queues them, called once initially and over time to stream more data
void lovrSourceStream(Source* source, ALuint* buffers, int count) {
  AudioStream* stream = source->stream;
  ALenum format = lovrSourceGetFormat(source);
  int frequency = stream->sampleRate;
  int samples = 0;
  int n = 0;

  // Keep decoding until there is nothing left to decode or all the buffers are filled
  while (n < count && (samples = lovrAudioStreamDecode(stream)) != 0) {
    alBufferData(buffers[n++], format, stream->buffer, samples * sizeof(ALshort), frequency);
  }

  alSourceQueueBuffers(source->id, n, buffers);

  if (samples == 0 && source->isLooping && n < count) {
    lovrAudioStreamRewind(stream);
    lovrSourceStream(source, buffers + n, count - n);
    return;
  }
}

int lovrSourceTell(Source* source) {
  int decoderOffset = lovrAudioStreamTell(source->stream);
  int samplesPerBuffer = source->stream->bufferSize / source->stream->channelCount / sizeof(ALshort);
  int queuedBuffers, sampleOffset;
  alGetSourcei(source->id, AL_BUFFERS_QUEUED, &queuedBuffers);
  alGetSourcei(source->id, AL_SAMPLE_OFFSET, &sampleOffset);

  int offset = decoderOffset - queuedBuffers * samplesPerBuffer + sampleOffset;

  if (offset < 0) {
    return offset + source->stream->samples;
  } else {
    return offset;
  }
}
