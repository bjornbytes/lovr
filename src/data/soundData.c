#include "data/soundData.h"
#include "lib/stb/stb_vorbis.h"

SoundData* lovrSoundDataCreate(int samples, int sampleRate, int bitDepth, int channelCount) {
  SoundData* soundData = lovrAlloc(sizeof(SoundData), lovrSoundDataDestroy);
  if (!soundData) return NULL;

  soundData->samples = samples;
  soundData->sampleRate = sampleRate;
  soundData->bitDepth = bitDepth;
  soundData->channelCount = channelCount;
  soundData->blob.size = samples * channelCount * (bitDepth / 8);
  soundData->blob.data = calloc(1, soundData->blob.size);

  return soundData;
}

SoundData* lovrSoundDataCreateFromAudioStream(AudioStream* audioStream) {
  SoundData* soundData = lovrAlloc(sizeof(SoundData), lovrSoundDataDestroy);
  if (!soundData) return NULL;

  soundData->samples = audioStream->samples;
  soundData->sampleRate = audioStream->sampleRate;
  soundData->bitDepth = audioStream->bitDepth;
  soundData->channelCount = audioStream->channelCount;
  soundData->blob.size = audioStream->samples * audioStream->channelCount * (audioStream->bitDepth / 8);
  soundData->blob.data = calloc(1, soundData->blob.size);

  int samples;
  short* buffer = soundData->blob.data;
  size_t offset = 0;
  lovrAudioStreamRewind(audioStream);
  while ((samples = lovrAudioStreamDecode(audioStream, buffer + offset, soundData->blob.size - (offset * sizeof(short)))) != 0) {
    offset += samples;
  }

  return soundData;
}

SoundData* lovrSoundDataCreateFromBlob(Blob* blob) {
  SoundData* soundData = lovrAlloc(sizeof(SoundData), lovrSoundDataDestroy);
  if (!soundData) return NULL;

  soundData->samples = stb_vorbis_decode_memory(blob->data, blob->size, &soundData->channelCount, &soundData->sampleRate, (short**) &soundData->blob.data);
  soundData->bitDepth = 16;

  return soundData;
}

void lovrSoundDataDestroy(void* ref) {
  lovrBlobDestroy(ref);
}
