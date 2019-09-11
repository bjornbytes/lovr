#include "data/soundData.h"
#include "util.h"
#include "lib/stb/stb_vorbis.h"
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "core/ref.h"

// SoundData

SoundData* lovrSoundDataInit(SoundData* soundData, int frames, int channels) {
  soundData->frames = frames;
  soundData->channels = channels;
  soundData->blob.size = frames * FRAME_SIZE(channels);
  soundData->blob.data = calloc(1, soundData->blob.size);
  lovrAssert(soundData->blob.data, "Out of memory");
  return soundData;
}

SoundData* lovrSoundDataInitFromBlob(SoundData* soundData, Blob* blob) {
  Decoder decoder;
  memset(&decoder, 0, sizeof(decoder));
  lovrDecoderInitOgg(&decoder, blob);
  soundData->channels = decoder.channels;

  Blob* raw = &soundData->blob;
  int frameLimit = 256;

  do {
    frameLimit *= 2;
    raw->size = frameLimit * FRAME_SIZE(soundData->channels);
    raw->data = realloc(raw->data, raw->size);
    lovrAssert(raw->data, "Out of memory");

    uint8_t* dest = (uint8_t*) raw->data + soundData->frames * FRAME_SIZE(soundData->channels);
    soundData->frames += lovrDecoderDecode(&decoder, frameLimit - soundData->frames, decoder.channels, dest);
  } while (soundData->frames == frameLimit);

  lovrDecoderDestroy(&decoder);
  return soundData;
}

void lovrSoundDataDestroy(void* ref) {
lovrBlobDestroy(ref);
}


// Raw decoder

static void lovrRawDecoderDestroy(Decoder* decoder) {
  lovrRelease(Blob, decoder->blob);
}

static uint32_t lovrRawDecoderDecode(Decoder* decoder, uint32_t frames, uint32_t channels, void* data) {
  lovrAssert(channels == decoder->channels, "Currently the raw audio decoder requires both input and output to have the same channel count"); // Currently only mono audio sources can be created so this is okay
  int stride = FRAME_SIZE(decoder->channels);
  frames = MIN(frames, decoder->frames - decoder->internal.i);
  memcpy(data, (uint8_t*) decoder->blob->data + decoder->internal.i * stride, frames * stride);
  decoder->internal.i += frames;
  return frames;
}

static void lovrRawDecoderSeek(Decoder* decoder, uint32_t frame) {
  decoder->internal.i = frame;
}

static uint32_t lovrRawDecoderTell(Decoder* decoder) {
  return decoder->internal.i;
}

// ogg decoder

static void lovrOggDecoderDestroy(Decoder* decoder) {
  stb_vorbis_close(decoder->internal.p);
  lovrRelease(Blob, decoder->blob);
}

static uint32_t lovrOggDecoderDecode(Decoder* decoder, uint32_t frames, uint32_t channels, void* data) {
  return stb_vorbis_get_samples_float_interleaved(decoder->internal.p, channels, data, frames * channels);
}

static void lovrOggDecoderSeek(Decoder* decoder, uint32_t frame) {
  stb_vorbis_seek(decoder->internal.p, frame);
}

static uint32_t lovrOggDecoderTell(Decoder* decoder) {
  return stb_vorbis_get_sample_offset(decoder->internal.p);
}

// Decoder

Decoder* lovrDecoderInitRaw(Decoder* decoder, SoundData* soundData) {
  decoder->blob = &soundData->blob;
  decoder->frames = soundData->frames;
  decoder->channels = soundData->channels;
  decoder->destroy = lovrRawDecoderDestroy;
  decoder->decode = lovrRawDecoderDecode;
  decoder->seek = lovrRawDecoderSeek;
  decoder->tell = lovrRawDecoderTell;
  lovrRetain(decoder->blob);
  return decoder;
}

Decoder* lovrDecoderInitOgg(Decoder* decoder, Blob* blob) {
  decoder->blob = blob;
  decoder->internal.p = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
  lovrAssert(decoder->internal.p, "Could not decode ogg audio from '%s'", blob->name);

  stb_vorbis_info info = stb_vorbis_get_info(decoder->internal.p);
  lovrAssert(info.sample_rate == 44100, "Audio data must have a sample rate of 44100, got %d in %s", info.sample_rate, blob->name);
  decoder->frames = stb_vorbis_stream_length_in_samples(decoder->internal.p);
  decoder->channels = info.channels;

  decoder->destroy = lovrOggDecoderDestroy;
  decoder->decode = lovrOggDecoderDecode;
  decoder->seek = lovrOggDecoderSeek;
  decoder->tell = lovrOggDecoderTell;
  lovrRetain(blob);
  return decoder;
}

void lovrDecoderDestroy(Decoder* decoder) {
  decoder->destroy(decoder);
}
