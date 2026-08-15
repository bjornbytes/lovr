#include "data/sound.h"
#include "data/blob.h"
#include "util.h"
#include "lib/stb/stb_vorbis.h"
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "lib/minimp3/minimp3_ex.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef uint32_t SoundCallback(Sound* sound, uint32_t offset, uint32_t count, void* data);

struct Sound {
  atomic_uint ref;
  SoundCallback* read;
  Blob* blob;
  void* decoder;
  SampleFormat format;
  uint32_t channels;
  uint32_t sampleRate;
  uint32_t frames;
  uint32_t cursor;
};

// Readers

static uint32_t lovrSoundReadRaw(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  uint8_t* p = sound->blob->data;
  uint32_t n = MIN(count, sound->frames - offset);
  size_t stride = lovrSoundGetStride(sound);
  memcpy(data, p + offset * stride, n * stride);
  return n;
}

static uint32_t lovrSoundReadStream(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  return lovrAudioStreamRead((AudioStream*) sound, count, data);
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
  uint32_t channels = lovrSoundGetChannelCount(sound);

  if (sound->cursor != offset) {
    mp3dec_ex_seek(sound->decoder, offset * channels);
    sound->cursor = offset;
  }

  size_t samples = mp3dec_ex_read(sound->decoder, data, count * channels);
  uint32_t frames = (uint32_t) (samples / channels);
  sound->cursor += frames;
  return frames;
}

// Sound

Sound* lovrSoundCreate(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate) {
  lovrCheck(channels < MAX_CHANNELS, "Max sound channels is %d", MAX_CHANNELS);
  Sound* sound = lovrCalloc(sizeof(Sound));
  sound->ref = 1;
  sound->frames = frames;
  sound->format = format;
  sound->channels = channels;
  sound->sampleRate = sampleRate;
  sound->read = lovrSoundReadRaw;
  size_t size = frames * lovrSoundGetStride(sound);
  void* data = lovrCalloc(size);
  sound->blob = lovrBlobCreate(data, size, "Sound");
  return sound;
}

static bool loadOgg(Sound** result, Blob* blob, bool decode) {
  if (blob->size < 4 || memcmp(blob->data, "OggS", 4)) return true;

  Sound* sound = lovrCalloc(sizeof(Sound));
  sound->ref = 1;
  sound->decoder = stb_vorbis_open_memory(blob->data, (int) blob->size, NULL, NULL);
  if (!sound->decoder) {
    lovrSetError("Could not load Ogg from '%s'", blob->name);
    lovrFree(sound);
  }

  stb_vorbis_info info = stb_vorbis_get_info(sound->decoder);
  sound->format = SAMPLE_F32;
  sound->channels = info.channels;
  sound->sampleRate = info.sample_rate;
  sound->frames = stb_vorbis_stream_length_in_samples(sound->decoder);

  if (decode) {
    sound->read = lovrSoundReadRaw;
    uint32_t channels = lovrSoundGetChannelCount(sound);
    if (sound->frames * channels > INT_MAX || channels > MAX_CHANNELS) {
      stb_vorbis_close(sound->decoder);
      lovrFree(sound);
      return lovrSetError("Decoded OGG file has too many samples/channels");
    }

    size_t size = sound->frames * lovrSoundGetStride(sound);
    void* data = lovrCalloc(size);
    sound->blob = lovrBlobCreate(data, size, "Sound");
    if (stb_vorbis_get_samples_float_interleaved(sound->decoder, channels, data, (int) size / sizeof(float)) < (int) sound->frames) {
      lovrRelease(sound->blob, lovrBlobDestroy);
      stb_vorbis_close(sound->decoder);
      lovrFree(sound);
      return lovrSetError("Could not decode vorbis from '%s'", blob->name);
    }

    stb_vorbis_close(sound->decoder);
    sound->decoder = NULL;
    *result = sound;
    return true;
  } else {
    sound->read = lovrSoundReadOgg;
    sound->blob = blob;
    lovrRetain(blob);
    *result = sound;
    return true;
  }
}

// The WAV importer supports:
// - 16, 24, 32 bit PCM or 32 bit floating point samples, uncompressed
// - WAVE_FORMAT_EXTENSIBLE format extension
// - mono (1), stereo (2), or first-order full-sphere ambisonic (4) channel layouts
// - Ambisonic formats:
//   - AMB: AMBISONIC_B_FORMAT extensible format GUIDs (Furse-Malham channel ordering/normalization)
//   - AmbiX: All other 4/9/16 channel files assume ACN channel ordering and SN3D normalization
static bool loadWAV(Sound** result, Blob* blob, bool decode) {
  if (blob->size < 64 || memcmp(blob->data, "RIFF", 4)) return true;

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
    return lovrSetError("Invalid WAV GUID");
  }

  lovrAssert(pcm || f32, "Invalid WAV sample format");
  lovrAssert(wav->channels == 1 || wav->channels == 2 || wav->channels == 4 || wav->channels == 9 || wav->channels == 16, "Invalid WAV channel count");

  Sound* sound = lovrCalloc(sizeof(Sound));
  sound->ref = 1;
  sound->format = f32 || wav->sampleSize == 24 || wav->sampleSize == 32 ? SAMPLE_F32 : SAMPLE_I16;
  sound->channels = wav->channels;
  sound->sampleRate = wav->sampleRate;

  // Search for data chunk containing samples
  size_t offset = 12 + 8 + wav->fmtSize;
  char* data = (char*) blob->data + offset;
  for (;;) {
    uint32_t size;
    memcpy(&size, data + 4, 4);
    if (!memcmp(data, "data", 4)) {
      if (offset + 8 + size > blob->size) {
        lovrFree(sound);
        return lovrSetError("Invalid WAV");
      }
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
  void* raw = lovrMalloc(bytes);
  if (pcm && wav->sampleSize == 24) {
    float* out = raw;
    const uint8_t* in = (const uint8_t*) data;
    for (size_t i = 0, j = 0; i < samples; i++, j += 3) {
      int32_t x = in[j + 2] & 0x80 ? 0xff : 0;
      x = (x << 8) | in[j + 2];
      x = (x << 8) | in[j + 1];
      x = (x << 8) | in[j + 0];
      out[i] = x * (1.f / 8388608.f);
    }
  } else if (pcm && wav->sampleSize == 32) {
    float* out = raw;
    const int32_t* in = (const int32_t*) data;
    for (size_t i = 0; i < samples; i++) {
      out[i] = in[i] * (1.f / 2147483648.f);
    }
  } else {
    memcpy(raw, data, bytes);
  }

  // Reorder/normalize Furse-Malham channels to ACN/SN3D
  // https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats
  if (amb) {
    // FuMa -> ACN channel map/scale
    uint32_t channelMap[] = { 0, 3, 1, 2, 6, 7, 5, 8, 4, 12, 13, 11, 14, 10, 15, 9 };
    float channelScale[] = {
      sqrtf(2.f),
      1.f,
      1.f,
      1.f,
      1.f,
      sqrtf(3.f) / 2.f,
      sqrtf(3.f) / 2.f,
      sqrtf(3.f) / 2.f,
      sqrtf(3.f) / 2.f,
      1.f,
      sqrtf(32.f / 45.f),
      sqrtf(32.f / 45.f),
      sqrtf(5.f) / 3.f,
      sqrtf(5.f) / 3.f,
      sqrtf(5.f / 8.f),
      sqrtf(5.f / 8.f)
    };

    if (sound->format == SAMPLE_I16) {
      short tmp[16];
      short* out = raw;
      for (uint32_t f = 0; f < sound->frames; f++) {
        memcpy(tmp, out, sound->channels * sizeof(short));
        for (uint32_t c = 0; c < sound->channels; c++) {
          out[channelMap[c]] = tmp[c] * channelScale[c] + .5f;
        }
        out += sound->channels;
      }
    } else if (sound->format == SAMPLE_F32) {
      float tmp[16];
      float* out = raw;
      for (uint32_t f = 0; f < sound->frames; f++) {
        memcpy(tmp, out, sound->channels * sizeof(float));
        for (uint32_t c = 0; c < sound->channels; c++) {
          out[channelMap[c]] = tmp[c] * channelScale[c];
        }
        out += sound->channels;
      }
    }
  }

  sound->blob = lovrBlobCreate(raw, bytes, blob->name);
  sound->read = lovrSoundReadRaw;
  *result = sound;
  return true;
}

static bool loadMP3(Sound** result, Blob* blob, bool decode) {
  if (mp3dec_detect_buf(blob->data, blob->size)) return true;

  if (decode) {
    mp3dec_t decoder;
    mp3dec_file_info_t info;
    int status = mp3dec_load_buf(&decoder, blob->data, blob->size, &info, NULL, NULL);
    lovrAssert(!status, "Could not decode mp3 from '%s'", blob->name);
    if (info.samples / info.channels > UINT32_MAX || info.channels > MAX_CHANNELS) {
      lovrFree(info.buffer);
      return lovrSetError("MP3 is too long or has too many channels");
    }

    Sound* sound = lovrCalloc(sizeof(Sound));
    sound->ref = 1;
    sound->blob = lovrBlobCreate(info.buffer, info.samples * sizeof(float), blob->name);
    sound->format = SAMPLE_F32;
    sound->sampleRate = info.hz;
    sound->channels = info.channels;
    sound->frames = (uint32_t) (info.samples / info.channels);
    sound->read = lovrSoundReadRaw;
    *result = sound;
    return true;
  } else {
    Sound* sound = lovrCalloc(sizeof(Sound));
    mp3dec_ex_t* decoder = sound->decoder = lovrMalloc(sizeof(mp3dec_ex_t));
    if (mp3dec_ex_open_buf(sound->decoder, blob->data, blob->size, MP3D_SEEK_TO_SAMPLE)) {
      lovrFree(sound->decoder);
      lovrFree(sound);
      return lovrSetError("Could not load mp3 from '%s'", blob->name);
    }
    sound->ref = 1;
    sound->format = SAMPLE_F32;
    sound->sampleRate = decoder->info.hz;
    sound->channels = decoder->info.channels;
    sound->frames = decoder->samples / decoder->info.channels;
    sound->read = lovrSoundReadMp3;
    sound->blob = blob;
    *result = sound;
    lovrRetain(blob);
    return true;
  }
}

Sound* lovrSoundLoad(Blob* blob, bool decode) {
  Sound* sound = NULL;
  if (!sound && !loadOgg(&sound, blob, decode)) return NULL;
  if (!sound && !loadWAV(&sound, blob, decode)) return NULL;
  if (!sound && !loadMP3(&sound, blob, decode)) return NULL;
  if (!sound) lovrSetError("Could not load sound from '%s': Audio format not recognized", blob->name);
  return sound;
}

void lovrSoundDestroy(void* ref) {
  Sound* sound = (Sound*) ref;
  lovrRelease(sound->blob, lovrBlobDestroy);
  if (sound->read == lovrSoundReadOgg) stb_vorbis_close(sound->decoder);
  if (sound->read == lovrSoundReadMp3) mp3dec_ex_close(sound->decoder), lovrFree(sound->decoder);
  lovrFree(sound);
}

Blob* lovrSoundGetBlob(Sound* sound) {
  return sound->blob;
}

SampleFormat lovrSoundGetFormat(Sound* sound) {
  return sound->format;
}

uint32_t lovrSoundGetChannelCount(Sound* sound) {
  return sound->channels;
}

uint32_t lovrSoundGetSampleRate(Sound* sound) {
  return sound->sampleRate;
}

uint32_t lovrSoundGetFrameCount(Sound* sound) {
  return sound->frames;
}

size_t lovrSoundGetStride(Sound* sound) {
  return lovrSoundGetChannelCount(sound) * (sound->format == SAMPLE_I16 ? sizeof(short) : sizeof(float));
}

bool lovrSoundIsCompressed(Sound* sound) {
  return sound->decoder;
}

bool lovrSoundIsStream(Sound* sound) {
  return sound->read == lovrSoundReadStream;
}

uint32_t lovrSoundRead(Sound* sound, uint32_t offset, uint32_t count, void* data) {
  return sound->read(sound, offset, count, data);
}

bool lovrSoundWrite(Sound* sound, uint32_t offset, uint32_t count, const void* data, uint32_t* framesWritten) {
  lovrCheck(!sound->decoder, "Compressed sounds can not be written to");
  size_t stride = lovrSoundGetStride(sound);
  count = MIN(count, sound->frames - offset);
  memcpy((char*) sound->blob->data + offset * stride, data, count * stride);
  if (framesWritten) *framesWritten = count;
  return true;
}

bool lovrSoundCopy(Sound* src, Sound* dst, uint32_t count, uint32_t srcOffset, uint32_t dstOffset, uint32_t* framesCopied) {
  lovrCheck(!dst->decoder, "Compressed sounds can not be written to");
  lovrCheck(src != dst, "Can not copy a Sound to itself");
  lovrCheck(src->format == dst->format, "Sound formats need to match");
  lovrCheck(src->channels == dst->channels, "Sound channel counts need to match");
  uint32_t frames = 0;

  count = MIN(count, dst->frames - dstOffset);
  size_t stride = lovrSoundGetStride(src);
  char* data = (char*) dst->blob->data + dstOffset * stride;
  while (frames < count) {
    uint32_t read = src->read(src, srcOffset + frames, count - frames, data);
    if (read == 0) break;
    data += read * stride;
    frames += read;
  }

  if (framesCopied) *framesCopied = frames;
  return true;
}

// AudioStream

struct AudioStream {
  Sound sound;
  _Alignas(64) atomic_uint read;
  _Alignas(64) atomic_uint write;
};

AudioStream* lovrAudioStreamCreate(uint32_t frames, SampleFormat format, uint32_t channels, uint32_t sampleRate) {
  AudioStream* stream = lovrCalloc(sizeof(AudioStream));
  stream->sound.ref = 1;
  stream->sound.frames = frames;
  stream->sound.format = format;
  stream->sound.channels = channels;
  stream->sound.sampleRate = sampleRate;
  stream->sound.read = lovrSoundReadStream;
  size_t size = frames * lovrSoundGetStride(&stream->sound);
  stream->sound.blob = lovrBlobCreate(lovrMalloc(size), size, "Sound");
  return stream;
}

void lovrAudioStreamDestroy(void* ref) {
  AudioStream* stream = ref;
  lovrSoundDestroy(&stream->sound);
}

Sound* lovrAudioStreamGetSound(AudioStream* stream) {
  return &stream->sound;
}

uint32_t lovrAudioStreamRead(AudioStream* stream, uint32_t frameCount, void* data) {
  uint32_t write = atomic_load_explicit(&stream->write, memory_order_acquire);
  uint32_t read = atomic_load_explicit(&stream->read, memory_order_relaxed);
  frameCount = MIN(frameCount, write - read);

  uint32_t readIndex = read % stream->sound.frames;
  uint32_t count = MIN(frameCount, stream->sound.frames - readIndex);
  size_t stride = lovrSoundGetStride(&stream->sound);
  char* src = stream->sound.blob->data;

  memcpy(data, src + readIndex * stride, count * stride);

  if (count < frameCount) {
    memcpy((char*) data + count * stride, src, (frameCount - count) * stride);
  }

  atomic_store_explicit(&stream->read, read + frameCount, memory_order_release);

  return frameCount;
}

uint32_t lovrAudioStreamWrite(AudioStream* stream, uint32_t frameCount, const void* data) {
  uint32_t read = atomic_load_explicit(&stream->read, memory_order_acquire);
  uint32_t write = atomic_load_explicit(&stream->write, memory_order_relaxed);
  frameCount = MIN(frameCount, stream->sound.frames - (write - read));

  uint32_t writeIndex = write % stream->sound.frames;
  uint32_t count = MIN(frameCount, stream->sound.frames - writeIndex);
  size_t stride = lovrSoundGetStride(&stream->sound);
  char* dst = stream->sound.blob->data;

  memcpy(dst + writeIndex * stride, data, count * stride);

  if (count < frameCount) {
    memcpy(dst, (char*) data + count * stride, (frameCount - count) * stride);
  }

  atomic_store_explicit(&stream->write, write + frameCount, memory_order_release);

  return frameCount;
}

uint32_t lovrAudioStreamGetReadCapacity(AudioStream* stream) {
  uint32_t write = atomic_load_explicit(&stream->write, memory_order_acquire);
  uint32_t read = atomic_load_explicit(&stream->read, memory_order_relaxed);
  return write - read;
}

uint32_t lovrAudioStreamGetWriteCapacity(AudioStream* stream) {
  uint32_t read = atomic_load_explicit(&stream->read, memory_order_acquire);
  uint32_t write = atomic_load_explicit(&stream->write, memory_order_relaxed);
  return stream->sound.frames - (write - read);
}
