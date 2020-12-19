#include "data/soundData.h"
#include "data/blob.h"
#include "core/util.h"
#include "core/ref.h"
#include "lib/stb/stb_vorbis.h"
#include "lib/miniaudio/miniaudio.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static const ma_format miniAudioFormatFromLovr[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

static const ma_format sampleSizes[] = {
  [SAMPLE_I16] = 2,
  [SAMPLE_F32] = 4
};

size_t SampleFormatBytesPerFrame(int channelCount, SampleFormat fmt)
{
  return channelCount * sampleSizes[fmt];
}

static uint32_t lovrSoundDataReadRaw(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  uint8_t* p = soundData->blob->data;
  uint32_t n = MIN(count, soundData->frames - offset);
  size_t stride = SampleFormatBytesPerFrame(soundData->channels, soundData->format);
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
  uint32_t channels = soundData->channels;
  float* p = data;
  int n;

  do {
    n = stb_vorbis_get_samples_float_interleaved(soundData->decoder, channels, p, count * channels);
    p += n * channels;
    frames += n;
    count -= n;
  } while (frames < count && n > 0);

  soundData->cursor += frames;
  return frames;
}

/*
static uint32_t lovrSoundDataReadMp3(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  return 0;
}
*/

static uint32_t lovrSoundDataReadRing(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  uint8_t *charData = (uint8_t*)data;
  size_t bytesPerFrame = SampleFormatBytesPerFrame(soundData->channels, soundData->format);
  size_t totalRead = 0;
  while(count > 0) {
    uint32_t availableFramesInRing = count;
    void *store;
    ma_result acquire_status = ma_pcm_rb_acquire_read(soundData->ring, &availableFramesInRing, &store);
    if (acquire_status != MA_SUCCESS) return 0;
    memcpy(charData, store, availableFramesInRing * bytesPerFrame);
    ma_result commit_status = ma_pcm_rb_commit_read(soundData->ring, availableFramesInRing, store);
    if (commit_status != MA_SUCCESS) return 0;

    if (availableFramesInRing == 0) {
      return totalRead;
    }

    count -= availableFramesInRing;
    charData += availableFramesInRing * bytesPerFrame;
    totalRead += availableFramesInRing;
  }
  return totalRead;
}


SoundData* lovrSoundDataCreateRaw(uint32_t frameCount, uint32_t channelCount, uint32_t sampleRate, SampleFormat format, struct Blob* blob) {
  lovrAssert(format != SAMPLE_INVALID, "Invalid format");
  SoundData* soundData = lovrAlloc(SoundData);
  soundData->format = format;
  soundData->sampleRate = sampleRate;
  soundData->channels = channelCount;
  soundData->frames = frameCount;
  soundData->read = lovrSoundDataReadRaw;

  if (blob) {
    soundData->blob = blob;
    lovrRetain(blob);
  } else {
    size_t size = frameCount * SampleFormatBytesPerFrame(channelCount, format);
    void* data = calloc(1, size);
    lovrAssert(data, "Out of memory");
    soundData->blob = lovrBlobCreate(data, size, "SoundData");
  }

  return soundData;
}

SoundData* lovrSoundDataCreateStream(uint32_t bufferSizeInFrames, uint32_t channels, uint32_t sampleRate, SampleFormat format) {
  lovrAssert(format != SAMPLE_INVALID, "Invalid format");
  SoundData* soundData = lovrAlloc(SoundData);
  soundData->format = format;
  soundData->sampleRate = sampleRate;
  soundData->channels = channels;
  soundData->frames = bufferSizeInFrames;
  soundData->read = lovrSoundDataReadRing;
  soundData->ring = calloc(1, sizeof(ma_pcm_rb));
  ma_result rbStatus = ma_pcm_rb_init(miniAudioFormatFromLovr[format], channels, bufferSizeInFrames, NULL, NULL, soundData->ring);
  lovrAssert(rbStatus == MA_SUCCESS, "Failed to create ring buffer for streamed SoundData: %s (%d)", ma_result_description(rbStatus), rbStatus);
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
      size_t size = soundData->frames * SampleFormatBytesPerFrame(soundData->channels, soundData->format);
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

size_t lovrSoundDataStreamAppendBlob(SoundData *dest, struct Blob* blob) {
  return lovrSoundDataStreamAppendBuffer(dest, blob->data, blob->size);
}

size_t lovrSoundDataStreamAppendBuffer(SoundData *dest, const void *buf, size_t byteSize) {
  lovrAssert(dest->ring, "Data can only be appended to a SoundData stream");

  const uint8_t *charBuf = (const uint8_t*)buf;
  void *store;
  size_t blobOffset = 0;
  size_t bytesPerFrame = SampleFormatBytesPerFrame(dest->channels, dest->format);
  size_t frameCount = byteSize / bytesPerFrame;
  size_t framesAppended = 0;
  while(frameCount > 0) {
    uint32_t availableFrames = frameCount;
    ma_result acquire_status = ma_pcm_rb_acquire_write(dest->ring, &availableFrames, &store);
    lovrAssert(acquire_status == MA_SUCCESS, "Failed to acquire ring buffer: %s (%d)", ma_result_description(acquire_status), acquire_status);
    memcpy(store, charBuf + blobOffset, availableFrames * bytesPerFrame);
    ma_result commit_status = ma_pcm_rb_commit_write(dest->ring, availableFrames, store);
    lovrAssert(commit_status == MA_SUCCESS, "Failed to commit to ring buffer: %s (%d)", ma_result_description(commit_status), commit_status);
    if (availableFrames == 0) {
      return framesAppended;
    }

    frameCount -= availableFrames;
    blobOffset += availableFrames * bytesPerFrame;
    framesAppended += availableFrames;
  }
  return framesAppended;
}

size_t lovrSoundDataStreamAppendSound(SoundData *dest, SoundData *src) {
  lovrAssert(dest->channels == src->channels && dest->sampleRate == src->sampleRate && dest->format == src->format, "Source and destination SoundData formats must match");
  lovrAssert(src->blob && src->read == lovrSoundDataReadRaw, "Source SoundData must have static PCM data and not be a stream");
  return lovrSoundDataStreamAppendBlob(dest, src->blob);
}

void lovrSoundDataSetSample(SoundData* soundData, size_t index, float value) {
  lovrAssert(soundData->blob && soundData->read == lovrSoundDataReadRaw, "Source SoundData must have static PCM data and not be a stream");
  lovrAssert(index < soundData->frames, "Sample index out of range");
  switch (soundData->format) {
    case SAMPLE_I16: ((int16_t*) soundData->blob->data)[index] = value * SHRT_MAX; break;
    case SAMPLE_F32: ((float*) soundData->blob->data)[index] = value; break;
    default: lovrThrow("Unsupported SoundData format %d\n", soundData->format); break;
  }
}

bool lovrSoundDataIsStream(SoundData *soundData) {
  return soundData->read == lovrSoundDataReadRing;
}

uint32_t lovrSoundDataGetDuration(SoundData *soundData)
{
  if (lovrSoundDataIsStream(soundData)) {
    return ma_pcm_rb_available_read(soundData->ring);
  } else {
    return soundData->frames;
  }
}

void lovrSoundDataDestroy(void* ref) {
  SoundData* soundData = (SoundData*) ref;
  stb_vorbis_close(soundData->decoder);
  lovrRelease(Blob, soundData->blob);
  ma_pcm_rb_uninit(soundData->ring);
  free(soundData->ring);
}
