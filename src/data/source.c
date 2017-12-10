#include "data/source.h"
#include "lib/stb/stb_vorbis.h"
#include "util.h"
#include <stdlib.h>

SourceData* lovrSourceDataCreate(Blob* blob) {
  SourceData* sourceData = malloc(sizeof(SourceData));
  if (!sourceData) return NULL;

  stb_vorbis* decoder = stb_vorbis_open_memory(blob->data, blob->size, NULL, NULL);

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
  sourceData->blob = blob;
  lovrRetain(&blob->ref);

  return sourceData;
}

void lovrSourceDataDestroy(SourceData* sourceData) {
  stb_vorbis_close(sourceData->decoder);
  lovrRelease(&sourceData->blob->ref);
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
