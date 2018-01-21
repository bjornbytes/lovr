#include "data/audioStream.h"
#include "lib/stb/stb_vorbis.h"
#include "util.h"
#include <stdlib.h>

AudioStream* lovrAudioStreamCreate(Blob* blob, size_t bufferSize) {
  AudioStream* stream = lovrAlloc(sizeof(AudioStream), lovrAudioStreamDestroy);
  if (!stream) return NULL;

  stb_vorbis* decoder = stb_vorbis_open_memory(blob->data, blob->size, NULL, NULL);

  if (!decoder) {
    free(stream);
    return NULL;
  }

  stb_vorbis_info info = stb_vorbis_get_info(decoder);

  stream->bitDepth = 16;
  stream->channelCount = info.channels;
  stream->sampleRate = info.sample_rate;
  stream->samples = stb_vorbis_stream_length_in_samples(decoder);
  stream->decoder = decoder;
  stream->bufferSize = stream->channelCount * bufferSize * sizeof(short);
  stream->buffer = malloc(stream->bufferSize);
  stream->blob = blob;
  lovrRetain(&blob->ref);

  return stream;
}

void lovrAudioStreamDestroy(const Ref* ref) {
  AudioStream* stream = containerof(ref, AudioStream);
  stb_vorbis_close(stream->decoder);
  lovrRelease(&stream->blob->ref);
  free(stream->buffer);
  free(stream);
}

int lovrAudioStreamDecode(AudioStream* stream) {
  stb_vorbis* decoder = (stb_vorbis*) stream->decoder;
  short* buffer = (short*) stream->buffer;
  int channelCount = stream->channelCount;
  int capacity = stream->bufferSize / sizeof(short);
  int samples = 0;

  while (samples < capacity) {
    int count = stb_vorbis_get_samples_short_interleaved(decoder, channelCount, buffer + samples, capacity - samples);
    if (count == 0) break;
    samples += count * channelCount;
  }

  return samples;
}

void lovrAudioStreamRewind(AudioStream* stream) {
  stb_vorbis* decoder = (stb_vorbis*) stream->decoder;
  stb_vorbis_seek_start(decoder);
}

void lovrAudioStreamSeek(AudioStream* stream, int sample) {
  stb_vorbis* decoder = (stb_vorbis*) stream->decoder;
  stb_vorbis_seek(decoder, sample);
}

int lovrAudioStreamTell(AudioStream* stream) {
  stb_vorbis* decoder = (stb_vorbis*) stream->decoder;
  return stb_vorbis_get_sample_offset(decoder);
}
