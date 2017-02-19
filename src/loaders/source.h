#pragma once

typedef struct {
  int bitDepth;
  int channels;
  int sampleRate;
  int samples;
  int bufferSize;
  void* buffer;
  void* decoder;
  void* data;
} SourceData;

SourceData* lovrSourceDataFromFile(void* data, int size);
void lovrSourceDataDestroy(SourceData* sourceData);
int lovrSourceDataDecode(SourceData* sourceData);
void lovrSourceDataRewind(SourceData* sourceData);
void lovrSourceDataSeek(SourceData* sourceData, int sample);
int lovrSourceDataTell(SourceData* sourceData);
