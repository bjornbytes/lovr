#include "data/soundData.h"
#include "data/blob.h"
#include "core/util.h"
#include "core/ref.h"
#include "lib/stb/stb_vorbis.h"
#include <stdlib.h>
#include <string.h>

static const size_t sampleSizes[] = {
  [SAMPLE_F32] = 4,
  [SAMPLE_I16] = 2
};

static uint32_t lovrSoundDataReadRaw(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  uint8_t* p = soundData->blob->data;
  uint32_t n = MIN(count, soundData->frames - offset);
  size_t stride = soundData->channels * sampleSizes[soundData->format];
  memcpy(data, p + offset * stride, n * stride);
  return n;
}

/*
static uint32_t lovrSoundDataReadWav(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  return 0;
}
*/

static uint32_t lovrSoundDataReadOgg(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  if (soundData->cursor != offset) {
    stb_vorbis_seek(soundData->decoder, (int) offset);
    soundData->cursor = offset;
  }

  uint32_t frames = 0;
  float* p = data;
  int n;

  do {
    n = stb_vorbis_get_samples_float_interleaved(soundData->decoder, soundData->channels, p, (int) (count - frames));
    p += n * soundData->channels;
    frames += n;
  } while (frames < count && n > 0);

  soundData->cursor += frames;
  return frames;
}

/*
static uint32_t lovrSoundDataReadMp3(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  return 0;
}
*/

static uint32_t lovrSoundDataReadQueue(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  return 0;
}

SoundData* lovrSoundDataCreate(uint32_t frameCount, uint32_t channelCount, uint32_t sampleRate, SampleFormat format, struct Blob* blob, uint32_t bufferLimit) {
  SoundData* soundData = lovrAlloc(SoundData);

  soundData->format = format;
  soundData->sampleRate = sampleRate;
  soundData->channels = channelCount;
  soundData->frames = frameCount;

  if (frameCount > 0) {
    soundData->read = lovrSoundDataReadRaw;
    if (blob) {
      soundData->blob = blob;
      lovrRetain(blob);
    } else {
      size_t size = frameCount * channelCount * sampleSizes[format];
      void* data = calloc(1, size);
      lovrAssert(data, "Out of memory");
      soundData->blob = lovrBlobCreate(data, size, "SoundData");
    }
  } else {
    soundData->read = lovrSoundDataReadQueue;
    soundData->buffers = bufferLimit ? bufferLimit : 8;
    soundData->queue = calloc(1, bufferLimit * sizeof(Blob*));
  }

  return soundData;
}

SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode) {
  SoundData* soundData = lovrAlloc(SoundData);

  if (blob->size >= 4 && !memcmp(blob->data, "OggS", 4)) {
    soundData->decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
    lovrAssert(soundData->decoder, "Could not load sound from '%s'", blob->name);

    stb_vorbis_info info = stb_vorbis_get_info(soundData->decoder);
    soundData->frames = stb_vorbis_stream_length_in_samples(soundData->decoder);
    soundData->sampleRate = info.sample_rate;
    soundData->channels = info.channels;
    soundData->format = SAMPLE_F32;

    if (decode) {
      soundData->read = lovrSoundDataReadRaw;
      size_t size = soundData->frames * soundData->channels * sampleSizes[soundData->format];
      void* data = calloc(1, size);
      lovrAssert(data, "Out of memory");
      soundData->blob = lovrBlobCreate(data, size, "SoundData");
      if (stb_vorbis_get_samples_float_interleaved(soundData->decoder, info.channels, data, size / 4) < (int) soundData->frames) {
        lovrThrow("Could not decode sound from '%s'", blob->name);
      }
      stb_vorbis_close(soundData->decoder);
      soundData->decoder = NULL;
    } else {
      soundData->read = lovrSoundDataReadOgg;
      soundData->blob = blob;
      lovrRetain(blob);
    }
  }

  return soundData;
}

void lovrSoundDataDestroy(void* ref) {
  SoundData* soundData = (SoundData*) ref;
  stb_vorbis_close(soundData->decoder);
  lovrRelease(Blob, soundData->blob);
  free(soundData->queue);
}
