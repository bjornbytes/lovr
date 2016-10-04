#include "modelData.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>

ModelData* lovrModelDataCreate(const char* filename) {
  ModelData* modelData = malloc(sizeof(ModelData));
  modelData->scene = aiImportFile(filename, aiProcessPreset_TargetRealtime_MaxQuality);
  return modelData;
}

void lovrModelDataDestroy(ModelData* modelData) {
  aiReleaseImport(modelData->scene);
  free(modelData);
}
