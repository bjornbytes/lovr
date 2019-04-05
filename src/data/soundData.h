#include "data/blob.h"
#include "types.h"
#include <stdint.h>

#pragma once

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE sizeof(float)
#define FRAME_SIZE(c) SAMPLE_SIZE * c

// SoundData

typedef struct SoundData {
  Blob blob;
  int frames;
  int channels;
} SoundData;

SoundData* lovrSoundDataInit(SoundData* soundData, int frames, int channels);
SoundData* lovrSoundDataInitFromBlob(SoundData* soundData, Blob* blob);
#define lovrSoundDataCreate(...) lovrSoundDataInit(lovrAlloc(SoundData), __VA_ARGS__)
#define lovrSoundDataCreateFromBlob(...) lovrSoundDataInitFromBlob(lovrAlloc(SoundData), __VA_ARGS__)
void lovrSoundDataDestroy(void* ref);

// Decoder

typedef struct Decoder {
  Ref ref;
  Blob* blob;
  int frames;
  int channels;
  union {
    void* p;
    uint32_t i;
  } internal;
  void (*destroy)(struct Decoder*);
  uint32_t (*decode)(struct Decoder*, uint32_t, void*);
  void (*seek)(struct Decoder*, uint32_t);
  uint32_t (*tell)(struct Decoder*);
} Decoder;

Decoder* lovrDecoderInitRaw(Decoder* decoder, SoundData* soundData);
Decoder* lovrDecoderInitOgg(Decoder* decoder, Blob* blob);
#define lovrDecoderCreateRaw(...) lovrDecoderInitRaw(lovrAlloc(Decoder), __VA_ARGS__)
#define lovrDecoderCreateOgg(...) lovrDecoderInitOgg(lovrAlloc(Decoder), __VA_ARGS__)
void lovrDecoderDestroy(Decoder* decoder);
#define lovrDecoderDecode(d, f, p) (d)->decode(d, f, p)
#define lovrDecoderSeek(d, f) (d)->seek(d, f)
#define lovrDecoderTell(d) (d)->tell(d)
