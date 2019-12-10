#include "data/soundData.h"
#include "data/audioStream.h"
#include "core/util.h"
#include "lib/stb/stb_vorbis.h"
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

SoundData* lovrSoundDataInit(SoundData* soundData, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channelCount) {
  soundData->samples = samples;
  soundData->sampleRate = sampleRate;
  soundData->bitDepth = bitDepth;
  soundData->channelCount = channelCount;
  soundData->blob.size = samples * channelCount * (bitDepth / 8);
  soundData->blob.data = calloc(1, soundData->blob.size);
  lovrAssert(soundData->blob.data, "Out of memory");
  return soundData;
}

SoundData* lovrSoundDataInitFromAudioStream(SoundData* soundData, AudioStream* audioStream) {
  soundData->samples = audioStream->samples;
  soundData->sampleRate = audioStream->sampleRate;
  soundData->bitDepth = audioStream->bitDepth;
  soundData->channelCount = audioStream->channelCount;
  soundData->blob.size = audioStream->samples * audioStream->channelCount * (audioStream->bitDepth / 8);
  soundData->blob.data = calloc(1, soundData->blob.size);
  lovrAssert(soundData->blob.data, "Out of memory");

  size_t samples;
  int16_t* buffer = soundData->blob.data;
  size_t offset = 0;
  lovrAudioStreamRewind(audioStream);
  while ((samples = lovrAudioStreamDecode(audioStream, buffer + offset, soundData->blob.size - (offset * sizeof(int16_t)))) != 0) {
    offset += samples;
  }

  return soundData;
}

SoundData* lovrSoundDataInitFromBlob(SoundData* soundData, Blob* blob) {
  int sampleRate, channels;
  soundData->bitDepth = 16;
  soundData->samples = stb_vorbis_decode_memory(blob->data, (int) blob->size, &channels, &sampleRate, (int16_t**) &soundData->blob.data);
  soundData->sampleRate = sampleRate;
  soundData->channelCount = channels;
  soundData->blob.size = soundData->samples * soundData->channelCount * (soundData->bitDepth / 8);
  return soundData;
}

float lovrSoundDataGetSample(SoundData* soundData, size_t index) {
  lovrAssert(index < soundData->blob.size / (soundData->bitDepth / 8), "Sample index out of range");
  switch (soundData->bitDepth) {
    case 8: return ((int8_t*) soundData->blob.data)[index] / (float) CHAR_MAX;
    case 16: return ((int16_t*) soundData->blob.data)[index] / (float) SHRT_MAX;
    default: lovrThrow("Unsupported SoundData bit depth %d\n", soundData->bitDepth); return 0;
  }
}

void lovrSoundDataSetSample(SoundData* soundData, size_t index, float value) {
  lovrAssert(index < soundData->blob.size / (soundData->bitDepth / 8), "Sample index out of range");
  switch (soundData->bitDepth) {
    case 8: ((int8_t*) soundData->blob.data)[index] = value * CHAR_MAX; break;
    case 16: ((int16_t*) soundData->blob.data)[index] = value * SHRT_MAX; break;
    default: lovrThrow("Unsupported SoundData bit depth %d\n", soundData->bitDepth); break;
  }
}

void lovrSoundDataDestroy(void* ref) {
  lovrBlobDestroy(ref);
}
