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
} SourceData;

SourceData* lovrSourceDataCreate(Blob* blob);
void lovrSourceDataDestroy(SourceData* sourceData);
int lovrSourceDataDecode(SourceData* sourceData);
void lovrSourceDataRewind(SourceData* sourceData);
void lovrSourceDataSeek(SourceData* sourceData, int sample);
int lovrSourceDataTell(SourceData* sourceData);
