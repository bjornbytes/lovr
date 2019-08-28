#include "data/blob.h"
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
#define lovrSoundDataCreateFromAudioStream(...) lovrSoundDataInitFromAudioStream(lovrAlloc(SoundData), __VA_ARGS__)
#define lovrSoundDataCreateFromBlob(...) lovrSoundDataInitFromBlob(lovrAlloc(SoundData), __VA_ARGS__)
float lovrSoundDataGetSample(SoundData* soundData, size_t index);
void lovrSoundDataSetSample(SoundData* soundData, size_t index, float value);
void lovrSoundDataDestroy(void* ref);


// Decoder

typedef struct Decoder {
  Blob* blob;
  int frames;
  int channels;
  union {
    void* p;
    uint32_t i;
  } internal;
  void (*destroy)(struct Decoder*);
  uint32_t (*decode)(struct Decoder*, uint32_t, uint32_t, void*); // Decoder, Frames, Channels, dataPointer
  void (*seek)(struct Decoder*, uint32_t); // Decoder, To
  uint32_t (*tell)(struct Decoder*);
} Decoder;

Decoder* lovrDecoderInitRaw(Decoder* decoder, SoundData* soundData);
Decoder* lovrDecoderInitOgg(Decoder* decoder, Blob* blob);
#define lovrDecoderCreateRaw(...) lovrDecoderInitRaw(lovrAlloc(Decoder), __VA_ARGS__)
#define lovrDecoderCreateOgg(...) lovrDecoderInitOgg(lovrAlloc(Decoder), __VA_ARGS__)
void lovrDecoderDestroy(Decoder* decoder);
// Warning: When lovrDecoderDecode is called, f*c samples will be written into the buffer. There must be enough room.
// The number returned is the number of frames written, not the number of samples.
#define lovrDecoderDecode(d, f, c, p) (d)->decode(d, f, c, p)
#define lovrDecoderSeek(d, f) (d)->seek(d, f)
#define lovrDecoderTell(d) (d)->tell(d)
