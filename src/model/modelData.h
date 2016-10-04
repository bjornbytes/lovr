#include <stdlib.h>
#include <assimp/scene.h>

#ifndef LOVR_MODEL_DATA_TYPES
#define LOVR_MODEL_DATA_TYPES
typedef struct {
  const struct aiScene* scene;
} ModelData;
#endif

ModelData* lovrModelDataCreate(const char* filename);
void lovrModelDataDestroy(ModelData* modelData);
