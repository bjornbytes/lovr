#include "data/sound.h"
#include "data/blob.h"
#include "util.h"
#include "lib/stb/stb_vorbis.h"
#include "lib/miniaudio/miniaudio.h"
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "lib/minimp3/minimp3_ex.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static const ma_format miniaudioFormats[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

struct Sound {
  uint32_t ref;
  SoundCallback* read;
  void* callbackMemo; // When using lovrSoundCreateFromCallback, any state the read callback uses should be stored here
  SoundDestroyCallback* callbackMemoDestroy; // This should be used to free the callbackMemo pointer (if appropriate)
  Blob* blob;
  void* decoder;
  void* stream;
  SampleFormat format;
  ChannelLayout layout;
  uint32_t sampleRate;
  uint32_t frames;
  uint32_t cursor;
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
  ma_pcm_rb_commit_read(sound->stream, frames);
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

  uint32_t channels = lovrSoundGetChannelCount(sound);
  size_t samples = mp3dec_ex_read(sound->decoder, data, count * channels);
  uint32_t frames = (uint32_t) (samples / channels);
  sound->cursor += frames;
  return frames;
}

// Sound

Sound* lovrSoundCreateRaw(uint32_t frames, SampleFormat format, ChannelLayout layout, uint32_t sampleRate, Blob* blob) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->frames = frames;
  sound->format = format;
  sound->layout = layout;
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

Sound* lovrSoundCreateStream(uint32_t frames, SampleFormat format, ChannelLayout layout, uint32_t sampleRate) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->frames = frames;
  sound->format = format;
  sound->layout = layout;
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

static bool loadOgg(Sound* sound, Blob* blob, bool decode) {
  if (blob->size < 4 || memcmp(blob->data, "OggS", 4)) return false;

  sound->decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
  lovrAssert(sound->decoder, "Could not load Ogg from '%s'", blob->name);

  stb_vorbis_info info = stb_vorbis_get_info(sound->decoder);
  sound->format = SAMPLE_F32;
  sound->layout = info.channels >= 2 ? CHANNEL_STEREO : CHANNEL_MONO;
  sound->sampleRate = info.sample_rate;
  sound->frames = stb_vorbis_stream_length_in_samples(sound->decoder);

  if (decode) {
    sound->read = lovrSoundReadRaw;
    uint32_t channels = lovrSoundGetChannelCount(sound);
    lovrAssert(sound->frames * channels <= INT_MAX, "Decoded OGG file has too many samples");
    size_t size = sound->frames * lovrSoundGetStride(sound);
    void* data = calloc(1, size);
    lovrAssert(data, "Out of memory");
    sound->blob = lovrBlobCreate(data, size, "Sound");
    if (stb_vorbis_get_samples_float_interleaved(sound->decoder, channels, data, (int) size / sizeof(float)) < (int) sound->frames) {
      lovrThrow("Could not decode vorbis from '%s'", blob->name);
    }
    stb_vorbis_close(sound->decoder);
    sound->decoder = NULL;
    return true;
  } else {
    sound->read = lovrSoundReadOgg;
    sound->blob = blob;
    lovrRetain(blob);
    return true;
  }
}

// The WAV importer supports:
// - 16, 24, 32 bit PCM or 32 bit floating point samples, uncompressed
// - WAVE_FORMAT_EXTENSIBLE format extension
// - mono (1), stereo (2), or first-order full-sphere ambisonic (4) channel layouts
// - Ambisonic formats:
//   - AMB: AMBISONIC_B_FORMAT extensible format GUIDs (Furse-Malham channel ordering/normalization)
//   - AmbiX: All other 4 channel files assume ACN channel ordering and SN3D normalization
static bool loadWAV(Sound* sound, Blob* blob, bool decode) {
  if (blob->size < 64 || memcmp(blob->data, "RIFF", 4)) return false;

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
    uint16_t validBits;
    uint32_t channelMask;
    char guid[16];
  } wav_t;

  uint8_t guidpcm[16] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };
  uint8_t guidf32[16] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };
  uint8_t guidpcmamb[16] = { 0x01, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1, 0xca, 0x00, 0x00, 0x00 };
  uint8_t guidf32amb[16] = { 0x03, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1, 0xca, 0x00, 0x00, 0x00 };

  wav_t* wav = blob->data;
  lovrAssert(wav->size == blob->size - 8, "Invalid WAV");
  lovrAssert(!memcmp(&wav->fileFormat, "WAVE", 4), "Invalid WAV");
  lovrAssert(!memcmp(&wav->fmtId, "fmt ", 4), "Invalid WAV");
  lovrAssert(wav->sampleSize == 16 || wav->sampleSize == 24 || wav->sampleSize == 32, "Invalid WAV sample size");
  bool extensible = wav->fmtSize == 40 && wav->extSize == 22 && wav->format == 65534;
  bool amb = extensible && (!memcmp(wav->guid, guidpcmamb, 16) || !memcmp(wav->guid, guidf32amb, 16));
  bool pcm = extensible ? wav->guid[0] == 0x01 : wav->format == 1;
  bool f32 = (extensible ? wav->guid[0] == 0x03 : wav->format == 3) && wav->sampleSize == 32;

  if (extensible && !amb && memcmp(wav->guid, guidpcm, 16) && memcmp(wav->guid, guidf32, 16)) {
    lovrThrow("Invalid WAV GUID");
  }

  lovrAssert(pcm || f32, "Invalid WAV sample format");
  lovrAssert(wav->channels != 9 && wav->channels != 16, "Invalid WAV channel count"" (Note: only first order ambisonics are supported)");
  lovrAssert(wav->channels == 1 || wav->channels == 2 || wav->channels == 4, "Invalid WAV channel count");

  sound->format = f32 || wav->sampleSize == 24 || wav->sampleSize == 32 ? SAMPLE_F32 : SAMPLE_I16;
  sound->layout = wav->channels == 4 ? CHANNEL_AMBISONIC : (wav->channels == 2 ? CHANNEL_STEREO : CHANNEL_MONO);
  sound->sampleRate = wav->sampleRate;

  // Search for data chunk containing samples
  size_t offset = 12 + 8 + wav->fmtSize;
  char* data = (char*) blob->data + offset;
  for (;;) {
    uint32_t size;
    memcpy(&size, data + 4, 4);
    if (!memcmp(data, "data", 4)) {
      lovrAssert(offset + 8 + size <= blob->size, "Invalid WAV");
      sound->frames = size / wav->frameSize;
      data += 8;
      break;
    } else if (offset + 8 + size > blob->size - 8) { // EOF
      return false;
    } else {
      offset += 8 + size;
      data += 8 + size;
    }
  }

  // Conversion
  size_t samples = sound->frames * lovrSoundGetChannelCount(sound);
  size_t bytes = sound->frames * lovrSoundGetStride(sound);
  void* raw = malloc(bytes);
  lovrAssert(raw, "Out of memory");
  if (pcm && wav->sampleSize == 24) {
    float* out = raw;
    const char* in = data;
    for (size_t i = 0; i < samples; i++) {
      int32_t x = ((in[i * 3 + 0] << 8) | (in[i * 3 + 1] << 16) | (in[i * 3 + 2] << 24)) >> 8;
      out[i] = x * (1. / 8388608.);
    }
  } else if (pcm && wav->sampleSize == 32) {
    float* out = raw;
    const int32_t* in = (const int32_t*) data;
    for (size_t i = 0; i < samples; i++) {
      out[i] = in[i] * (1. / 2147483648.);
    }
  } else {
    memcpy(raw, data, bytes);
  }

  // Reorder/normalize Furse-Malham channels to ACN/SN3D
  if (amb) {
    if (sound->format == SAMPLE_I16) {
      short* f = raw;
      for (size_t i = 0; i < samples; i += 4) {
        short tmp = f[1];
        f[0] = f[0] * 1.414213562 + .5;
        f[1] = f[2];
        f[2] = f[3];
        f[3] = tmp;
      }
    } else if (sound->format == SAMPLE_F32) {
      float* f = raw;
      for (size_t i = 0; i < samples; i += 4) {
        float tmp = f[1];
        f[0] = f[0] * 1.414213562f;
        f[1] = f[2];
        f[2] = f[3];
        f[3] = tmp;
      }
    }
  }

  sound->blob = lovrBlobCreate(raw, bytes, blob->name);
  sound->read = lovrSoundReadRaw;
  return true;
}

static bool loadMP3(Sound* sound, Blob* blob, bool decode) {
  if (mp3dec_detect_buf(blob->data, blob->size)) return false;

  if (decode) {
    mp3dec_t decoder;
    mp3dec_file_info_t info;
    int status = mp3dec_load_buf(&decoder, blob->data, blob->size, &info, NULL, NULL);
    lovrAssert(!status, "Could not decode mp3 from '%s'", blob->name);
    lovrAssert(info.samples / info.channels <= UINT32_MAX, "MP3 is too long");
    sound->blob = lovrBlobCreate(info.buffer, info.samples * sizeof(float), blob->name);
    sound->format = SAMPLE_F32;
    sound->sampleRate = info.hz;
    sound->layout = info.channels == 2 ? CHANNEL_STEREO : CHANNEL_MONO;
    sound->frames = (uint32_t) (info.samples / info.channels);
    sound->read = lovrSoundReadRaw;
    return true;
  } else {
    mp3dec_ex_t* decoder = sound->decoder = malloc(sizeof(mp3dec_ex_t));
    lovrAssert(decoder, "Out of memory");
    if (mp3dec_ex_open_buf(sound->decoder, blob->data, blob->size, MP3D_SEEK_TO_SAMPLE)) {
      free(sound->decoder);
      lovrThrow("Could not load mp3 from '%s'", blob->name);
    }
    sound->format = SAMPLE_F32;
    sound->sampleRate = decoder->info.hz;
    sound->layout = decoder->info.channels == 2 ? CHANNEL_STEREO : CHANNEL_MONO;
    sound->frames = decoder->samples / decoder->info.channels;
    sound->read = lovrSoundReadMp3;
    sound->blob = blob;
    lovrRetain(blob);
    return true;
  }
}

Sound* lovrSoundCreateFromFile(Blob* blob, bool decode) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;

  if (loadOgg(sound, blob, decode)) return sound;
  if (loadWAV(sound, blob, decode)) return sound;
  if (loadMP3(sound, blob, decode)) return sound;

  lovrThrow("Could not load sound from '%s': Audio format not recognized", blob->name);
}

Sound* lovrSoundCreateFromCallback(SoundCallback read, void *callbackMemo, SoundDestroyCallback callbackMemoDestroy, SampleFormat format, uint32_t sampleRate, ChannelLayout layout, uint32_t maxFrames) {
  Sound* sound = calloc(1, sizeof(Sound));
  lovrAssert(sound, "Out of memory");
  sound->ref = 1;
  sound->read = read;
  sound->format = format;
  sound->sampleRate = sampleRate;
  sound->layout = layout;
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
  return sound->layout;
}

uint32_t lovrSoundGetChannelCount(Sound* sound) {
  return 1 << sound->layout;
}

uint32_t lovrSoundGetSampleRate(Sound* sound) {
  return sound->sampleRate;
}

uint32_t lovrSoundGetFrameCount(Sound* sound) {
  return sound->stream ? ma_pcm_rb_available_read(sound->stream) : sound->frames;
}

uint32_t lovrSoundGetCapacity(Sound* sound) {
  return sound->stream ? ma_pcm_rb_available_write(sound->stream) : sound->frames;
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
      ma_pcm_rb_commit_write(sound->stream, chunk);
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
  lovrAssert(src->layout == dst->layout, "Sound channel layouts need to match");
  uint32_t frames = 0;

  if (dst->stream) {
    while (frames < count) {
      void* data;
      uint32_t available = count - frames;
      ma_pcm_rb_acquire_write(dst->stream, &available, &data);
      uint32_t read = src->read(src, srcOffset + frames, available, data);
      ma_pcm_rb_commit_write(dst->stream, read);
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

void *lovrSoundGetCallbackMemo(Sound* sound) {
  return sound->callbackMemo;
}
