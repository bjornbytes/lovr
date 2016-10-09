#include "modelData.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>

ModelData* lovrModelDataCreate(const char* filename) {
  ModelData* modelData = malloc(sizeof(ModelData));
  const struct aiScene* scene = aiImportFile(filename, aiProcessPreset_TargetRealtime_MaxQuality);
  const struct aiMesh* mesh = scene->mMeshes[0];
  modelData->vertexCount = mesh->mNumFaces * 3;
  modelData->data = malloc(3 * modelData->vertexCount * sizeof(float));
  for (int i = 0; i < mesh->mNumFaces; i++) {
    for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++) {
      int index = mesh->mFaces[i].mIndices[j];
      modelData->data[9 * i + 3 * j + 0] = mesh->mVertices[index].x;
      modelData->data[9 * i + 3 * j + 1] = mesh->mVertices[index].y;
      modelData->data[9 * i + 3 * j + 2] = mesh->mVertices[index].z;
    }
  }
  aiReleaseImport(scene);
  return modelData;
}

void lovrModelDataDestroy(ModelData* modelData) {
  free(modelData->data);
  free(modelData);
}
