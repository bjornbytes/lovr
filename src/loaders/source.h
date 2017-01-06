#include "audio/source.h"

SoundData* lovrSoundDataFromFile(void* data, int size);
void lovrSoundDataDestroy(SoundData* soundData);
int lovrSoundDataDecode(SoundData* soundData);
void lovrSoundDataRewind(SoundData* soundData);
void lovrSoundDataSeek(SoundData* soundData, int sample);
int lovrSoundDataTell(SoundData* soundData);
