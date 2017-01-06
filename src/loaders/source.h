#include "audio/source.h"

SoundData* lovrSoundDataFromFile(void* data, int size);
void lovrSoundDataDestroy(SoundData* soundData);
int lovrSoundDataDecode(SoundData* soundData);
void lovrSoundDataRewind(SoundData* soundData);
