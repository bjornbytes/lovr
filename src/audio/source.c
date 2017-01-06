#include "audio/source.h"
#include "audio/audio.h"
#include "loaders/source.h"

static ALenum lovrSourceGetState(Source* source) {
  ALenum state;
  alGetSourcei(source->id, AL_SOURCE_STATE, &state);
  return state;
}

Source* lovrSourceCreate(SoundData* soundData) {
  Source* source = lovrAlloc(sizeof(Source), lovrSourceDestroy);
  if (!source) return NULL;

  source->soundData = soundData;
  source->isLooping = 0;
  alGenSources(1, &source->id);
  alGenBuffers(SOURCE_BUFFERS, source->buffers);

  return source;
}

void lovrSourceDestroy(const Ref* ref) {
  Source* source = containerof(ref, Source);
  alDeleteSources(1, &source->id);
  alDeleteBuffers(SOURCE_BUFFERS, source->buffers);
  lovrSoundDataDestroy(source->soundData);
  free(source);
}

int lovrSourceGetBitDepth(Source* source) {
  return source->soundData->bitDepth;
}

int lovrSourceGetChannels(Source* source) {
  return source->soundData->channels;
}

int lovrSourceGetDuration(Source* source) {
  return source->soundData->samples;
}

// Get the OpenAL sound format for the sound
ALenum lovrSourceGetFormat(Source* source) {
  int channels = source->soundData->channels;
  int bitDepth = source->soundData->bitDepth;

  if (bitDepth == 8 && channels == 1) {
    return AL_FORMAT_MONO8;
  } else if (bitDepth == 8 && channels == 2) {
    return AL_FORMAT_STEREO8;
  } else if (bitDepth == 16 && channels == 1) {
    return AL_FORMAT_MONO16;
  } else if (bitDepth == 16 && channels == 2) {
    return AL_FORMAT_STEREO16;
  }

  return 0;
}

float lovrSourceGetPitch(Source* source) {
  float pitch;
  alGetSourcef(source->id, AL_PITCH, &pitch);
  return pitch;
}

void lovrSourceGetPosition(Source* source, float* x, float* y, float* z) {
  alGetSource3f(source->id, AL_POSITION, x, y, z);
}

int lovrSourceGetSampleRate(Source* source) {
  return source->soundData->sampleRate;
}

float lovrSourceGetVolume(Source* source) {
  float volume;
  alGetSourcef(source->id, AL_GAIN, &volume);
  return volume;
}

int lovrSourceIsLooping(Source* source) {
  return source->isLooping;
}

int lovrSourceIsPaused(Source* source) {
  return lovrSourceGetState(source) == AL_PAUSED;
}

int lovrSourceIsPlaying(Source* source) {
  return lovrSourceGetState(source) == AL_PLAYING;
}

int lovrSourceIsStopped(Source* source) {
  return lovrSourceGetState(source) == AL_STOPPED;
}

void lovrSourcePause(Source* source) {
  alSourcePause(source->id);
}

void lovrSourcePlay(Source* source) {
  if (lovrSourceIsPlaying(source)) {
    return;
  } else if (lovrSourceIsPaused(source)) {
    return lovrSourceResume(source);
  }

  lovrSourceStream(source, source->buffers, SOURCE_BUFFERS);
  alSourcePlay(source->id);
  lovrAudioPlay(source);
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

  int wasPaused = lovrSourceIsPaused(source);
  alSourceRewind(source->id);
  lovrSourceStop(source);
  lovrSourcePlay(source);
  if (wasPaused) {
    lovrSourcePause(source);
  }
}

void lovrSourceSeek(Source* source, int sample) {
  int wasPaused = lovrSourceIsPaused(source);
  lovrSourceStop(source);
  lovrSoundDataSeek(source->soundData, sample);
  lovrSourcePlay(source);
  if (wasPaused) {
    lovrSourcePause(source);
  }
}

void lovrSourceSetPitch(Source* source, float pitch) {
  alSourcef(source->id, AL_PITCH, pitch);
}

void lovrSourceSetLooping(Source* source, int isLooping) {
  source->isLooping = isLooping;
}

void lovrSourceSetPosition(Source* source, float x, float y, float z) {
  alSource3f(source->id, AL_POSITION, x, y, z);
}

void lovrSourceSetVolume(Source* source, float volume) {
  alSourcef(source->id, AL_GAIN, volume);
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
  lovrSoundDataRewind(source->soundData);
}

// Fills buffers with data and queues them, called once initially and over time to stream more data
void lovrSourceStream(Source* source, ALuint* buffers, int count) {
  SoundData* soundData = source->soundData;
  ALenum format = lovrSourceGetFormat(source);
  int frequency = soundData->sampleRate;
  int samples = 0;
  int n = 0;

  // Keep decoding until there is nothing left to decode or all the buffers are filled
  while (n < count && (samples = lovrSoundDataDecode(soundData)) != 0) {
    alBufferData(buffers[n++], format, soundData->buffer, samples * sizeof(ALshort), frequency);
  }

  alSourceQueueBuffers(source->id, n, buffers);

  if (samples == 0 && source->isLooping && n < count) {
    lovrSoundDataRewind(soundData);
    return lovrSourceStream(source, buffers + n, count - n);
  }
}

int lovrSourceTell(Source* source) {
  int decoderOffset = lovrSoundDataTell(source->soundData);
  int samplesPerBuffer = source->soundData->bufferSize / source->soundData->channels / sizeof(ALshort);
  int queuedBuffers, sampleOffset;
  alGetSourcei(source->id, AL_BUFFERS_QUEUED, &queuedBuffers);
  alGetSourcei(source->id, AL_SAMPLE_OFFSET, &sampleOffset);

  int offset = decoderOffset - queuedBuffers * samplesPerBuffer + sampleOffset;

  if (offset < 0) {
    return offset + source->soundData->samples;
  } else {
    return offset;
  }
}
