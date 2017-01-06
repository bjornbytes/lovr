#include "loaders/source.h"
#include "vendor/stb/stb_vorbis.h"
#include <stdlib.h>

SoundData* lovrSoundDataFromFile(void* data, int size) {
  SoundData* soundData = malloc(sizeof(SoundData));
  if (!soundData) return NULL;

  stb_vorbis* decoder = stb_vorbis_open_memory(data, size, NULL, NULL);

  if (!decoder) {
    free(soundData);
    return NULL;
  }

  stb_vorbis_info info = stb_vorbis_get_info(decoder);

  soundData->bitDepth = 16;
  soundData->channels = info.channels;
  soundData->sampleRate = info.sample_rate;
  soundData->samples = stb_vorbis_stream_length_in_samples(decoder);
  soundData->decoder = decoder;
  soundData->bufferSize = soundData->channels * 4096 * sizeof(short);
  soundData->buffer = malloc(soundData->bufferSize);

  return soundData;
}

void lovrSoundDataDestroy(SoundData* soundData) {
  stb_vorbis_close(soundData->decoder);
  free(soundData->buffer);
  free(soundData);
}

int lovrSoundDataDecode(SoundData* soundData) {
  stb_vorbis* decoder = (stb_vorbis*) soundData->decoder;
  short* buffer = (short*) soundData->buffer;
  int channels = soundData->channels;
  int capacity = soundData->bufferSize / sizeof(short);
  int samples = 0;

  while (samples < capacity) {
    int count = stb_vorbis_get_samples_short_interleaved(decoder, channels, buffer + samples, capacity - samples);
    if (count == 0) break;
    samples += count * channels;
  }

  return samples;
}

void lovrSoundDataRewind(SoundData* soundData) {
  stb_vorbis* decoder = (stb_vorbis*) soundData->decoder;
  stb_vorbis_seek_start(decoder);
}

void lovrSoundDataSeek(SoundData* soundData, int sample) {
  stb_vorbis* decoder = (stb_vorbis*) soundData->decoder;
  stb_vorbis_seek(decoder, sample);
}

int lovrSoundDataTell(SoundData* soundData) {
  stb_vorbis* decoder = (stb_vorbis*) soundData->decoder;
  return stb_vorbis_get_sample_offset(decoder);
}
