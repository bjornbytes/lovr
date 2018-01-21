#include "filesystem/blob.h"

#pragma once

typedef struct {
  int bitDepth;
  int channelCount;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
  Blob* blob;
} AudioStream;

AudioStream* lovrAudioStreamCreate(Blob* blob);
void lovrAudioStreamDestroy(AudioStream* stream);
int lovrAudioStreamDecode(AudioStream* stream);
void lovrAudioStreamRewind(AudioStream* stream);
void lovrAudioStreamSeek(AudioStream* stream, int sample);
int lovrAudioStreamTell(AudioStream* stream);
