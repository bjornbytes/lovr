#include "loaders/model.h"
#include "math/mat4.h"
#include <stdlib.h>
#include <assimp/cimport.h>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/postprocess.h>

static void assimpSumChildren(struct aiNode* assimpNode, int* totalChildren) {
  (*totalChildren)++;
  for (unsigned int i = 0; i < assimpNode->mNumChildren; i++) {
    assimpSumChildren(assimpNode->mChildren[i], totalChildren);
  }
}

static void assimpNodeTraversal(ModelData* modelData, struct aiNode* assimpNode, int* nodeId) {
  int currentIndex = *nodeId;
  ModelNode* node = &modelData->nodes[currentIndex];

  // Transform
  struct aiMatrix4x4 m = assimpNode->mTransformation;
  aiTransposeMatrix4(&m);
  mat4_set(node->transform, (float*) &m);

  // Primitives
  vec_init(&node->primitives);
  vec_pusharr(&node->primitives, assimpNode->mMeshes, assimpNode->mNumMeshes);

  // Children
  vec_init(&node->children);
  for (unsigned int n = 0; n < assimpNode->mNumChildren; n++) {
    (*nodeId)++;
    vec_push(&node->children, *nodeId);
    ModelNode* child = &modelData->nodes[*nodeId];
    child->parent = currentIndex;
    assimpNodeTraversal(modelData, assimpNode->mChildren[n], nodeId);
  }
}

ModelData* lovrModelDataCreate(Blob* blob) {
  ModelData* modelData = malloc(sizeof(ModelData));
  if (!modelData) return NULL;

  struct aiPropertyStore* propertyStore = aiCreatePropertyStore();
  aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
  const struct aiScene* scene = aiImportFileFromMemoryWithProperties(blob->data, blob->size, flags, NULL, propertyStore);
  aiReleasePropertyStore(propertyStore);

  modelData->nodeCount = 0;
  modelData->vertexCount = 0;
  modelData->indexCount = 0;
  modelData->hasNormals = 0;
  modelData->hasUVs = 0;

  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    modelData->vertexCount += assimpMesh->mNumVertices;
    modelData->indexCount += assimpMesh->mNumFaces * 3;
    modelData->hasNormals |= assimpMesh->mNormals != NULL;
    modelData->hasUVs |= assimpMesh->mTextureCoords[0] != NULL;
  }

  // Allocate
  modelData->primitiveCount = scene->mNumMeshes;
  modelData->primitives = malloc(modelData->primitiveCount * sizeof(ModelPrimitive));
  modelData->vertexSize = 3 + (modelData->hasNormals ? 3 : 0) + (modelData->hasUVs ? 2 : 0);
  modelData->vertices = malloc(modelData->vertexSize * modelData->vertexCount * sizeof(float));
  modelData->indices = malloc(modelData->indexCount * sizeof(uint32_t));

  // Load
  int vertex = 0;
  int index = 0;
  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    modelData->primitives[m].drawStart = index;
    modelData->primitives[m].drawCount = 0;

    // Indices
    for (unsigned int f = 0; f < assimpMesh->mNumFaces; f++) {
      struct aiFace assimpFace = assimpMesh->mFaces[f];
      lovrAssert(assimpFace.mNumIndices == 3, "Only triangular faces are supported");

      modelData->primitives[m].drawCount += assimpFace.mNumIndices;

      for (unsigned int i = 0; i < assimpFace.mNumIndices; i++) {
        modelData->indices[index++] = (vertex / modelData->vertexSize) + assimpFace.mIndices[i];
      }
    }

    // Vertices
    for (unsigned int v = 0; v < assimpMesh->mNumVertices; v++) {
      modelData->vertices[vertex++] = assimpMesh->mVertices[v].x;
      modelData->vertices[vertex++] = assimpMesh->mVertices[v].y;
      modelData->vertices[vertex++] = assimpMesh->mVertices[v].z;

      if (modelData->hasNormals) {
        if (assimpMesh->mNormals) {
          modelData->vertices[vertex++] = assimpMesh->mNormals[v].x;
          modelData->vertices[vertex++] = assimpMesh->mNormals[v].y;
          modelData->vertices[vertex++] = assimpMesh->mNormals[v].z;
        } else {
          modelData->vertices[vertex++] = 0;
          modelData->vertices[vertex++] = 0;
          modelData->vertices[vertex++] = 0;
        }
      }

      if (modelData->hasUVs) {
        if (assimpMesh->mTextureCoords[0]) {
          modelData->vertices[vertex++] = assimpMesh->mTextureCoords[0][v].x;
          modelData->vertices[vertex++] = assimpMesh->mTextureCoords[0][v].y;
        } else {
          modelData->vertices[vertex++] = 0;
          modelData->vertices[vertex++] = 0;
        }
      }
    }
  }

  // Nodes
  modelData->nodeCount = 0;
  assimpSumChildren(scene->mRootNode, &modelData->nodeCount);
  modelData->nodes = malloc(modelData->nodeCount * sizeof(ModelNode));
  modelData->nodes[0].parent = -1;
  int nodeIndex = 0;
  assimpNodeTraversal(modelData, scene->mRootNode, &nodeIndex);

  aiReleaseImport(scene);
  return modelData;
}

void lovrModelDataDestroy(ModelData* modelData) {
for (int i = 0; i < modelData->nodeCount; i++) {
    vec_deinit(&modelData->nodes[i].children);
    vec_deinit(&modelData->nodes[i].primitives);
  }

  free(modelData->nodes);
  free(modelData->primitives);
  free(modelData->vertices);
  free(modelData->indices);
  free(modelData);
}
