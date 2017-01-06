#include "loaders/source.h"
#include "vendor/stb/stb_vorbis.h"
#include <stdlib.h>

SourceData* lovrSourceDataFromFile(void* data, int size) {
  SourceData* sourceData = malloc(sizeof(SourceData));
  if (!sourceData) return NULL;

  stb_vorbis* decoder = stb_vorbis_open_memory(data, size, NULL, NULL);

  if (!decoder) {
    free(sourceData);
    return NULL;
  }

  stb_vorbis_info info = stb_vorbis_get_info(decoder);

  sourceData->bitDepth = 16;
  sourceData->channels = info.channels;
  sourceData->sampleRate = info.sample_rate;
  sourceData->samples = stb_vorbis_stream_length_in_samples(decoder);
  sourceData->decoder = decoder;
  sourceData->bufferSize = sourceData->channels * 4096 * sizeof(short);
  sourceData->buffer = malloc(sourceData->bufferSize);

  return sourceData;
}

void lovrSourceDataDestroy(SourceData* sourceData) {
  stb_vorbis_close(sourceData->decoder);
  free(sourceData->buffer);
  free(sourceData);
}

int lovrSourceDataDecode(SourceData* sourceData) {
  stb_vorbis* decoder = (stb_vorbis*) sourceData->decoder;
  short* buffer = (short*) sourceData->buffer;
  int channels = sourceData->channels;
  int capacity = sourceData->bufferSize / sizeof(short);
  int samples = 0;

  while (samples < capacity) {
    int count = stb_vorbis_get_samples_short_interleaved(decoder, channels, buffer + samples, capacity - samples);
    if (count == 0) break;
    samples += count * channels;
  }

  return samples;
}

void lovrSourceDataRewind(SourceData* sourceData) {
  stb_vorbis* decoder = (stb_vorbis*) sourceData->decoder;
  stb_vorbis_seek_start(decoder);
}

void lovrSourceDataSeek(SourceData* sourceData, int sample) {
  stb_vorbis* decoder = (stb_vorbis*) sourceData->decoder;
  stb_vorbis_seek(decoder, sample);
}

int lovrSourceDataTell(SourceData* sourceData) {
  stb_vorbis* decoder = (stb_vorbis*) sourceData->decoder;
  return stb_vorbis_get_sample_offset(decoder);
}
