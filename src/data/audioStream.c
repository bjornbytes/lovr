#include "data/audioStream.h"
#include "data/blob.h"
#include "lib/stb/stb_vorbis.h"
#include "util.h"
#include <stdlib.h>

AudioStream* lovrAudioStreamInit(AudioStream* stream, Blob* blob, int bufferSize) {
  stb_vorbis* decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
  lovrAssert(decoder, "Could not create audio stream for '%s'", blob->name);

  stb_vorbis_info info = stb_vorbis_get_info(decoder);

  stream->bitDepth = 16;
  stream->channelCount = info.channels;
  stream->sampleRate = info.sample_rate;
  stream->samples = stb_vorbis_stream_length_in_samples(decoder);
  stream->decoder = decoder;
  stream->bufferSize = stream->channelCount * bufferSize * sizeof(short);
  stream->buffer = malloc(stream->bufferSize);
  lovrAssert(stream->buffer, "Out of memory");
  stream->blob = blob;
  lovrRetain(blob);

  return stream;
}

void lovrAudioStreamDestroy(void* ref) {
  AudioStream* stream = ref;
  stb_vorbis_close(stream->decoder);
  lovrRelease(Blob, stream->blob);
  free(stream->buffer);
  free(stream);
}

int lovrAudioStreamDecode(AudioStream* stream, short* destination, int size) {
  stb_vorbis* decoder = (stb_vorbis*) stream->decoder;
  short* buffer = destination ? destination : (short*) stream->buffer;
  int capacity = destination ? size : (stream->bufferSize / sizeof(short));
  int channelCount = stream->channelCount;
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
