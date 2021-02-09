#include "data/soundData.h"
#include "data/blob.h"
#include "core/util.h"
#include "lib/stb/stb_vorbis.h"
#include "lib/miniaudio/miniaudio.h"
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "lib/minimp3/minimp3_ex.h"
#include <stdlib.h>
#include <string.h>

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

struct SoundData {
  ref_t ref;
  uint32_t (*read)(SoundData* soundData, uint32_t offset, uint32_t count, void* data);
  struct Blob* blob;
  void* decoder;
  void* stream;
  SampleFormat format;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t frames;
  uint32_t cursor;
};

// Readers

static uint32_t lovrSoundDataReadRaw(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  uint8_t* p = soundData->blob->data;
  uint32_t n = MIN(count, soundData->frames - offset);
  size_t stride = lovrSoundDataGetStride(soundData);
  memcpy(data, p + offset * stride, n * stride);
  return n;
}

static uint32_t lovrSoundDataReadStream(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  void* p = NULL;
  uint32_t frames = count;
  ma_pcm_rb_acquire_read(soundData->stream, &frames, &p);
  memcpy(data, p, frames * lovrSoundDataGetStride(soundData));
  ma_pcm_rb_commit_read(soundData->stream, frames, p);
  return frames;
}

static uint32_t lovrSoundDataReadOgg(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  if (soundData->cursor != offset) {
    stb_vorbis_seek(soundData->decoder, (int) offset);
    soundData->cursor = offset;
  }

  uint32_t channels = soundData->channels;
  uint32_t n = stb_vorbis_get_samples_float_interleaved(soundData->decoder, channels, data, count * channels);
  soundData->cursor += n;
  return n;
}

static uint32_t lovrSoundDataReadMp3(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  if (soundData->cursor != offset) {
    mp3dec_ex_seek(soundData->decoder, offset);
    soundData->cursor = offset;
  }

  size_t samples = mp3dec_ex_read(soundData->decoder, data, count * soundData->channels);
  uint32_t frames = samples / soundData->channels;
  soundData->cursor += frames;
  return frames;
}

// SoundData

SoundData* lovrSoundDataCreateRaw(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate, struct Blob* blob) {
  SoundData* soundData = calloc(1, sizeof(SoundData));
  lovrAssert(soundData, "Out of memory");
  soundData->ref = 1;
  soundData->frames = frames;
  soundData->format = format;
  soundData->channels = channels;
  soundData->sampleRate = sampleRate;
  soundData->read = lovrSoundDataReadRaw;
  size_t size = frames * lovrSoundDataGetStride(soundData);
  void* data = calloc(1, size);
  lovrAssert(data, "Out of memory");
  soundData->blob = lovrBlobCreate(data, size, "SoundData");

  if (blob) {
    memcpy(soundData->blob->data, blob->data, MIN(size, blob->size));
  }

  return soundData;
}

SoundData* lovrSoundDataCreateStream(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate) {
  SoundData* soundData = calloc(1, sizeof(SoundData));
  lovrAssert(soundData, "Out of memory");
  soundData->ref = 1;
  soundData->frames = frames;
  soundData->format = format;
  soundData->channels = channels;
  soundData->sampleRate = sampleRate;
  soundData->read = lovrSoundDataReadStream;
  soundData->stream = malloc(sizeof(ma_pcm_rb));
  lovrAssert(soundData->stream, "Out of memory");
  size_t size = frames * lovrSoundDataGetStride(soundData);
  void* data = malloc(size);
  lovrAssert(data, "Out of memory");
  soundData->blob = lovrBlobCreate(data, size, NULL);
  ma_result status = ma_pcm_rb_init(miniaudioFormats[format], channels, frames, data, NULL, soundData->stream);
  lovrAssert(status == MA_SUCCESS, "Failed to create ring buffer for streamed SoundData: %s (%d)", ma_result_description(status), status);
  return soundData;
}

SoundData* lovrSoundDataCreateFromFile(struct Blob* blob, bool decode) {
  SoundData* soundData = calloc(1, sizeof(SoundData));
  lovrAssert(soundData, "Out of memory");
  soundData->ref = 1;

  if (blob->size >= 4 && !memcmp(blob->data, "OggS", 4)) {
    soundData->decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
    lovrAssert(soundData->decoder, "Could not load sound from '%s'", blob->name);

    stb_vorbis_info info = stb_vorbis_get_info(soundData->decoder);
    soundData->format = SAMPLE_F32;
    soundData->channels = info.channels;
    soundData->sampleRate = info.sample_rate;
    soundData->frames = stb_vorbis_stream_length_in_samples(soundData->decoder);

    if (decode) {
      soundData->read = lovrSoundDataReadRaw;
      size_t size = soundData->frames * lovrSoundDataGetStride(soundData);
      void* data = calloc(1, size);
      lovrAssert(data, "Out of memory");
      soundData->blob = lovrBlobCreate(data, size, "SoundData");
      if (stb_vorbis_get_samples_float_interleaved(soundData->decoder, info.channels, data, size / 4) < (int) soundData->frames) {
        lovrThrow("Could not decode vorbis from '%s'", blob->name);
      }
      stb_vorbis_close(soundData->decoder);
      soundData->decoder = NULL;
    } else {
      soundData->read = lovrSoundDataReadOgg;
      soundData->blob = blob;
      lovrRetain(blob);
    }

    return soundData;
  } else if (blob->size >= 64 && !memcmp(blob->data, "RIFF", 4)) {
    typedef struct {
      uint32_t id;
      uint32_t size;
      uint32_t fileFormat;
      uint32_t fmtId;
      uint32_t fmtSize;
      uint16_t format;
      uint16_t channels;
      uint32_t sampleRate;
      uint32_t byteRate;
      uint16_t frameSize;
      uint16_t sampleSize;
      uint16_t extSize;
      uint16_t validBitsPerSample;
      uint32_t channelMask;
      char guid[16];
    } wavHeader;

    char guidi16[16] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };
    char guidf32[16] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };

    wavHeader* wav = blob->data;
    lovrAssert(wav->size == blob->size - 8, "Invalid WAV");
    lovrAssert(!memcmp(&wav->fileFormat, "WAVE", 4), "Invalid WAV");
    lovrAssert(!memcmp(&wav->fmtId, "fmt ", 4), "Invalid WAV");
    if (wav->fmtSize == 16 && wav->format == 1 && wav->sampleSize == 16) {
      soundData->format = SAMPLE_I16;
    } else if (wav->fmtSize == 16 && wav->format == 3 && wav->sampleSize == 32) {
      soundData->format = SAMPLE_F32;
    } else if (wav->fmtSize == 40 && wav->format == 65534 && wav->extSize == 22 && wav->validBitsPerSample == 16 && !memcmp(wav->guid, guidi16, 16)) {
      soundData->format = SAMPLE_I16;
    } else if (wav->fmtSize == 40 && wav->format == 65534 && wav->extSize == 22 && wav->validBitsPerSample == 32 && !memcmp(wav->guid, guidf32, 16)) {
      soundData->format = SAMPLE_F32;
    } else {
      lovrThrow("Unsupported WAV format");
    }
    soundData->channels = wav->channels;
    soundData->sampleRate = wav->sampleRate;
    lovrAssert(wav->frameSize == lovrSoundDataGetStride(soundData), "Invalid WAV");

    size_t offset = 12 + 8 + wav->fmtSize;
    char* data = (char*) blob->data + offset;
    while (offset < blob->size - 8) {
      uint32_t chunkSize = *((uint32_t*) data + 1);
      if (!memcmp(data, "data", 4)) {
        offset += 8;
        data += 8;
        lovrAssert(chunkSize == blob->size - offset, "Invalid WAV");
        size_t size = blob->size - offset;
        void* samples = malloc(size); // TODO Blob views
        lovrAssert(samples, "Out of memory");
        memcpy(samples, data, size);
        soundData->blob = lovrBlobCreate(samples, size, blob->name);
        soundData->read = lovrSoundDataReadRaw;
        soundData->frames = size / wav->frameSize;
        return soundData;
      } else {
        offset += chunkSize + 8;
        data += chunkSize + 8;
      }
    }
  } else if (!mp3dec_detect_buf(blob->data, blob->size)) {
    if (decode) {
      mp3dec_t decoder;
      mp3dec_file_info_t info;
      int status = mp3dec_load_buf(&decoder, blob->data, blob->size, &info, NULL, NULL);
      lovrAssert(!status, "Could not decode mp3 from '%s'", blob->name);
      soundData->blob = lovrBlobCreate(info.buffer, info.samples * sizeof(float), blob->name);
      soundData->format = SAMPLE_F32;
      soundData->sampleRate = info.hz;
      soundData->channels = info.channels;
      soundData->frames = info.samples / info.channels;
      soundData->read = lovrSoundDataReadRaw;
      return soundData;
    } else {
      mp3dec_ex_t* decoder = soundData->decoder = malloc(sizeof(mp3dec_ex_t));
      lovrAssert(decoder, "Out of memory");
      if (mp3dec_ex_open_buf(soundData->decoder, blob->data, blob->size, MP3D_SEEK_TO_SAMPLE)) {
        free(soundData->decoder);
        lovrThrow("Could not load mp3 from '%s'", blob->name);
      }
      soundData->format = SAMPLE_F32;
      soundData->sampleRate = decoder->info.hz;
      soundData->channels = decoder->info.channels;
      soundData->frames = decoder->samples / soundData->channels;
      soundData->read = lovrSoundDataReadMp3;
      soundData->blob = blob;
      lovrRetain(blob);
      return soundData;
    }
  }

  lovrThrow("Could not load sound from '%s': Audio format not recognized", blob->name);
  return NULL;
}

void lovrSoundDataDestroy(void* ref) {
  SoundData* soundData = (SoundData*) ref;
  lovrRelease(soundData->blob, lovrBlobDestroy);
  if (soundData->read == lovrSoundDataReadOgg) stb_vorbis_close(soundData->decoder);
  if (soundData->read == lovrSoundDataReadMp3) mp3dec_ex_close(soundData->decoder), free(soundData->decoder);
  ma_pcm_rb_uninit(soundData->stream);
  free(soundData->stream);
  free(soundData);
}

Blob* lovrSoundDataGetBlob(SoundData* soundData) {
  return soundData->blob;
}

SampleFormat lovrSoundDataGetFormat(SoundData* soundData) {
  return soundData->format;
}

uint32_t lovrSoundDataGetChannelCount(SoundData* soundData) {
  return soundData->channels;
}

uint32_t lovrSoundDataGetSampleRate(SoundData* soundData) {
  return soundData->sampleRate;
}

uint32_t lovrSoundDataGetFrameCount(SoundData* soundData) {
  return soundData->stream ? ma_pcm_rb_available_read(soundData->stream) : soundData->frames;
}

size_t lovrSoundDataGetStride(SoundData* soundData) {
  return soundData->channels * (soundData->format == SAMPLE_I16 ? sizeof(short) : sizeof(float));
}

bool lovrSoundDataIsCompressed(SoundData* soundData) {
  return soundData->decoder;
}

bool lovrSoundDataIsStream(SoundData* soundData) {
  return soundData->stream;
}

uint32_t lovrSoundDataRead(SoundData* soundData, uint32_t offset, uint32_t count, void* data) {
  return soundData->read(soundData, offset, count, data);
}

uint32_t lovrSoundDataWrite(SoundData* soundData, uint32_t offset, uint32_t count, const void* data) {
  lovrAssert(!soundData->decoder, "Compressed SoundData can not be written to");
  size_t stride = lovrSoundDataGetStride(soundData);
  uint32_t frames = 0;

  if (soundData->stream) {
    const char* bytes = data;
    while (frames < count) {
      void* pointer;
      uint32_t chunk = count - frames;
      ma_pcm_rb_acquire_write(soundData->stream, &chunk, &pointer);
      memcpy(pointer, bytes, chunk * stride);
      ma_pcm_rb_commit_write(soundData->stream, chunk, pointer);
      if (chunk == 0) break;
      bytes += chunk * stride;
      frames += chunk;
    }
  } else {
    count = MIN(count, soundData->frames - offset);
    memcpy((char*) soundData->blob->data + offset * stride, data, count * stride);
    frames = count;
  }

  return frames;
}

uint32_t lovrSoundDataCopy(SoundData* src, SoundData* dst, uint32_t count, uint32_t srcOffset, uint32_t dstOffset) {
  lovrAssert(!dst->decoder, "Compressed SoundData can not be written to");
  lovrAssert(src != dst, "Can not copy a SoundData to itself");
  lovrAssert(src->format == dst->format, "SoundData formats need to match");
  lovrAssert(src->channels == dst->channels, "SoundData channel layouts need to match");
  uint32_t frames = 0;

  if (dst->stream) {
    while (frames < count) {
      void* data;
      uint32_t available = count - frames;
      ma_pcm_rb_acquire_write(dst->stream, &available, &data);
      uint32_t read = src->read(src, srcOffset + frames, available, data);
      ma_pcm_rb_commit_write(dst->stream, read, data);
      if (read == 0) break;
      frames += read;
    }
  } else {
    count = MIN(count, dst->frames - dstOffset);
    size_t stride = lovrSoundDataGetStride(src);
    char* data = (char*) dst->blob->data + dstOffset * stride;
    while (frames < count) {
      uint32_t read = src->read(src, srcOffset + frames, count - frames, data);
      if (read == 0) break;
      data += read * stride;
      frames += read;
    }
  }

  return frames;
}
