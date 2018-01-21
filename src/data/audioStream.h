#include "filesystem/blob.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  int bitDepth;
  int channelCount;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
  Blob* blob;
} AudioStream;

AudioStream* lovrAudioStreamCreate(Blob* blob, size_t bufferSize);
void lovrAudioStreamDestroy(const Ref* ref);
int lovrAudioStreamDecode(AudioStream* stream);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, int sample);
int lovrAudioStreamTell(AudioStream* stream);
