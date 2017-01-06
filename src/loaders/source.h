#include "audio/source.h"

SourceData* lovrSourceDataFromFile(void* data, int size);
void lovrSourceDataDestroy(SourceData* sourceData);
int lovrSourceDataDecode(SourceData* sourceData);
void lovrSourceDataRewind(SourceData* sourceData);
void lovrSourceDataSeek(SourceData* sourceData, int sample);
int lovrSourceDataTell(SourceData* sourceData);
