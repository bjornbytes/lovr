#include "audio/source.h"

SoundData* lovrSoundDataFromFile(void* data, int size);
void lovrSoundDataDestroy(SoundData* soundData);
int lovrSoundDataDecode(SoundData* soundData);
int lovrSoundDataRewind(SoundData* soundData);
