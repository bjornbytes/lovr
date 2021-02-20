#include "data/sound.h"
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

struct Sound {
  uint32_t ref;
  SoundCallback* read;
  struct Blob* blob;
  void* decoder;
  void* stream;
  SampleFormat format;
  ChannelLayout channels;
  uint32_t sampleRate;
  uint32_t frames;
  uint32_t cursor;
  void* callbackMemo; // When using lovrSoundCreateFromCallback, any state the read callback uses should be stored here
  SoundDestroyCallback* callbackMemoDestroy; // This should be used to free the callbackMemo pointer (if appropriate)
};

// Readers

static uint32_t lovrSoundReadRaw(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  uint8_t* p = sound->blob->data;
  uint32_t n = sound->frames == LOVR_SOUND_ENDLESS ? count : MIN(count, sound->frames - offset);
  size_t stride = lovrSoundGetStride(sound);
  memcpy(data, p + offset * stride, n * stride);
  return n;
}

static uint32_t lovrSoundReadStream(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  void* p = NULL;
  uint32_t frames = count;
  ma_pcm_rb_acquire_read(sound->stream, &frames, &p);
  memcpy(data, p, frames * lovrSoundGetStride(sound));
  ma_pcm_rb_commit_read(sound->stream, frames, p);
  return frames;
}

static uint32_t lovrSoundReadOgg(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  if (sound->cursor != offset) {
    stb_vorbis_seek(sound->decoder, (int) offset);
    sound->cursor = offset;
  }

  uint32_t channelCount = lovrSoundGetChannelCount(sound);
  uint32_t sampleCount = count * channelCount;
  uint32_t n = stb_vorbis_get_samples_float_interleaved(sound->decoder, channelCount, data, sampleCount);
  sound->cursor += n;
  return n;
}

static uint32_t lovrSoundReadMp3(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  if (sound->cursor != offset) {
    mp3dec_ex_seek(sound->decoder, offset);
    sound->cursor = offset;
  }

  size_t samples = mp3dec_ex_read(sound->decoder, data, count * lovrSoundGetChannelCount(sound));
  uint32_t frames = samples / sound->channels;
  sound->cursor += frames;
  return frames;
}

// Sound

Sound* lovrSoundCreateRaw(uint32_t frames, SampleFormat format, ChannelLayout channels, uint32_t sampleRate, struct Blob* blob) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->frames = frames;
  sound->format = format;
  sound->channels = channels;
  sound->sampleRate = sampleRate;
  sound->read = lovrSoundReadRaw;
  size_t size = frames * lovrSoundGetStride(sound);
  void* data = calloc(1, size);
  lovrAssert(data, "Out of memory");
  sound->blob = lovrBlobCreate(data, size, "Sound");

  if (blob) {
    memcpy(sound->blob->data, blob->data, MIN(size, blob->size));
  }

  return sound;
}

Sound* lovrSoundCreateStream(uint32_t frames, SampleFormat format, ChannelLayout channels, uint32_t sampleRate) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->frames = frames;
  sound->format = format;
  sound->channels = channels;
  sound->sampleRate = sampleRate;
  sound->read = lovrSoundReadStream;
  sound->stream = malloc(sizeof(ma_pcm_rb));
  lovrAssert(sound->stream, "Out of memory");
  size_t size = frames * lovrSoundGetStride(sound);
  void* data = malloc(size);
  lovrAssert(data, "Out of memory");
  sound->blob = lovrBlobCreate(data, size, NULL);
  ma_result status = ma_pcm_rb_init(miniaudioFormats[format], lovrSoundGetChannelCount(sound), frames, data, NULL, sound->stream);
  lovrAssert(status == MA_SUCCESS, "Failed to create ring buffer for streamed Sound: %s (%d)", ma_result_description(status), status);
  return sound;
}

Sound* lovrSoundCreateFromFile(struct Blob* blob, bool decode) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;

  if (blob->size >= 4 && !memcmp(blob->data, "OggS", 4)) {
    sound->decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
    lovrAssert(sound->decoder, "Could not load sound from '%s'", blob->name);

    stb_vorbis_info info = stb_vorbis_get_info(sound->decoder);
    sound->format = SAMPLE_F32;
    sound->channels = info.channels >= 2 ? CHANNEL_STEREO : CHANNEL_MONO;
    sound->sampleRate = info.sample_rate;
    sound->frames = stb_vorbis_stream_length_in_samples(sound->decoder);

    if (decode) {
      sound->read = lovrSoundReadRaw;
      size_t size = sound->frames * lovrSoundGetStride(sound);
      void* data = calloc(1, size);
      lovrAssert(data, "Out of memory");
      sound->blob = lovrBlobCreate(data, size, "Sound");
      if (stb_vorbis_get_samples_float_interleaved(sound->decoder, lovrSoundGetChannelCount(sound), data, size / sizeof(float)) < (int) sound->frames) {
        lovrThrow("Could not decode vorbis from '%s'", blob->name);
      }
      stb_vorbis_close(sound->decoder);
      sound->decoder = NULL;
    } else {
      sound->read = lovrSoundReadOgg;
      sound->blob = blob;
      lovrRetain(blob);
    }

    return sound;
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

    // Format
    if (wav->fmtSize == 16 && wav->format == 1 && wav->sampleSize == 16) {
      sound->format = SAMPLE_I16;
    } else if (wav->fmtSize == 16 && wav->format == 3 && wav->sampleSize == 32) {
      sound->format = SAMPLE_F32;
    } else if (wav->fmtSize == 40 && wav->format == 65534 && wav->extSize == 22 && wav->validBitsPerSample == 16 && !memcmp(wav->guid, guidi16, 16)) {
      sound->format = SAMPLE_I16;
    } else if (wav->fmtSize == 40 && wav->format == 65534 && wav->extSize == 22 && wav->validBitsPerSample == 32 && !memcmp(wav->guid, guidf32, 16)) {
      sound->format = SAMPLE_F32;
    } else {
      lovrThrow("Unsupported WAV format");
    }

    // Channels
    if (wav->channels == 2) {
      sound->channels = CHANNEL_STEREO;
    } else if (wav->channels == 1) {
      sound->channels = CHANNEL_MONO;
    } else {
      lovrThrow("Unsupported WAV channel layout");
    }

    sound->sampleRate = wav->sampleRate;
    lovrAssert(wav->frameSize == lovrSoundGetStride(sound), "Invalid WAV");

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
        sound->blob = lovrBlobCreate(samples, size, blob->name);
        sound->read = lovrSoundReadRaw;
        sound->frames = size / wav->frameSize;
        return sound;
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
      sound->blob = lovrBlobCreate(info.buffer, info.samples * sizeof(float), blob->name);
      sound->format = SAMPLE_F32;
      sound->sampleRate = info.hz;
      sound->channels = info.channels == 2 ? CHANNEL_STEREO : CHANNEL_MONO;
      sound->frames = info.samples / info.channels;
      sound->read = lovrSoundReadRaw;
      return sound;
    } else {
      mp3dec_ex_t* decoder = sound->decoder = malloc(sizeof(mp3dec_ex_t));
      lovrAssert(decoder, "Out of memory");
      if (mp3dec_ex_open_buf(sound->decoder, blob->data, blob->size, MP3D_SEEK_TO_SAMPLE)) {
        free(sound->decoder);
        lovrThrow("Could not load mp3 from '%s'", blob->name);
      }
      sound->format = SAMPLE_F32;
      sound->sampleRate = decoder->info.hz;
      sound->channels = decoder->info.channels == 2 ? CHANNEL_STEREO : CHANNEL_MONO;
      sound->frames = decoder->samples / decoder->info.channels;
      sound->read = lovrSoundReadMp3;
      sound->blob = blob;
      lovrRetain(blob);
      return sound;
    }
  }

  lovrThrow("Could not load sound from '%s': Audio format not recognized", blob->name);
  return NULL;
}

Sound* lovrSoundCreateFromCallback(SoundCallback read, void *callbackMemo, SoundDestroyCallback callbackMemoDestroy, SampleFormat format, uint32_t sampleRate, ChannelLayout channels, uint32_t maxFrames) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->read = read;
  sound->format = format;
  sound->sampleRate = sampleRate;
  sound->channels = channels;
  sound->frames = maxFrames;
  sound->callbackMemo = callbackMemo;
  sound->callbackMemoDestroy = callbackMemoDestroy;
  return sound;
}

void lovrSoundDestroy(void* ref) {
  Sound* sound = (Sound*) ref;
  if (sound->callbackMemoDestroy) sound->callbackMemoDestroy(sound);
  lovrRelease(sound->blob, lovrBlobDestroy);
  if (sound->read == lovrSoundReadOgg) stb_vorbis_close(sound->decoder);
  if (sound->read == lovrSoundReadMp3) mp3dec_ex_close(sound->decoder), free(sound->decoder);
  ma_pcm_rb_uninit(sound->stream);
  free(sound->stream);
  free(sound);
}

Blob* lovrSoundGetBlob(Sound* sound) {
  return sound->blob;
}

SampleFormat lovrSoundGetFormat(Sound* sound) {
  return sound->format;
}

ChannelLayout lovrSoundGetChannelLayout(Sound* sound) {
  return sound->channels;
}

uint32_t lovrSoundGetChannelCount(Sound* sound) {
  return 1 << sound->channels;
}

uint32_t lovrSoundGetSampleRate(Sound* sound) {
  return sound->sampleRate;
}

uint32_t lovrSoundGetFrameCount(Sound* sound) {
  return sound->stream ? ma_pcm_rb_available_read(sound->stream) : sound->frames;
}

size_t lovrSoundGetStride(Sound* sound) {
  return lovrSoundGetChannelCount(sound) * (sound->format == SAMPLE_I16 ? sizeof(short) : sizeof(float));
}

bool lovrSoundIsCompressed(Sound* sound) {
  return sound->decoder;
}

bool lovrSoundIsStream(Sound* sound) {
  return sound->stream;
}

uint32_t lovrSoundRead(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  return sound->read(sound, offset, count, data);
}

uint32_t lovrSoundWrite(Sound* sound, uint32_t offset, uint32_t count, const void* data) {
  lovrAssert(!sound->decoder, "Compressed Sound can not be written to");
  lovrAssert(sound->stream || sound->blob, "Live-generated sound can not be written to");
  size_t stride = lovrSoundGetStride(sound);
  uint32_t frames = 0;

  if (sound->stream) {
    const char* bytes = data;
    while (frames < count) {
      void* pointer;
      uint32_t chunk = count - frames;
      ma_pcm_rb_acquire_write(sound->stream, &chunk, &pointer);
      memcpy(pointer, bytes, chunk * stride);
      ma_pcm_rb_commit_write(sound->stream, chunk, pointer);
      if (chunk == 0) break;
      bytes += chunk * stride;
      frames += chunk;
    }
  } else {
    count = MIN(count, sound->frames - offset);
    memcpy((char*) sound->blob->data + offset * stride, data, count * stride);
    frames = count;
  }

  return frames;
}

uint32_t lovrSoundCopy(Sound* src, Sound* dst, uint32_t count, uint32_t srcOffset, uint32_t dstOffset) {
  lovrAssert(!dst->decoder, "Compressed Sound can not be written to");
  lovrAssert(dst->stream || dst->blob, "Live-generated sound can not be written to");
  lovrAssert(src != dst, "Can not copy a Sound to itself");
  lovrAssert(src->format == dst->format, "Sound formats need to match");
  lovrAssert(src->channels == dst->channels, "Sound channel layouts need to match");
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
    size_t stride = lovrSoundGetStride(src);
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

void *lovrSoundGetCallbackMemo(Sound *sound) {
  return sound->callbackMemo;
}
