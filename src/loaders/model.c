#include "loaders/model.h"
#include "filesystem/filesystem.h"
#include "math/mat4.h"
#include <libgen.h>
#include <stdlib.h>
#include <assimp/cfileio.h>
#include <assimp/cimport.h>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/postprocess.h>

// Blob IO (to avoid reading data twice)
static unsigned long assimpBlobRead(struct aiFile* file, char* buffer, size_t size, size_t count) {
  Blob* blob = (Blob*) file->UserData;
  char* data = blob->data;
  memcpy(buffer, data, count * size * sizeof(char));
  return count * size * sizeof(char);
}

static size_t assimpBlobGetSize(struct aiFile* file) {
  Blob* blob = (Blob*) file->UserData;
  return blob->size;
}

static aiReturn assimpBlobSeek(struct aiFile* file, unsigned long position, enum aiOrigin origin) {
  lovrThrow("Seek is not implemented for Blobs");
  return aiReturn_FAILURE;
}

static unsigned long assimpBlobTell(struct aiFile* file) {
  lovrThrow("Tell is not implemented for Blobs");
  return 0;
}

// File IO (for reading referenced materials/textures)
static unsigned long assimpFileRead(struct aiFile* file, char* buffer, size_t size, size_t count) {
  size_t bytesRead;
  lovrFilesystemFileRead(file->UserData, buffer, count, size, &bytesRead);
  return bytesRead;
}

static size_t assimpFileGetSize(struct aiFile* file) {
  return lovrFilesystemGetSize(file->UserData);
}

static aiReturn assimpFileSeek(struct aiFile* file, unsigned long position, enum aiOrigin origin) {
  return lovrFilesystemSeek(file->UserData, position) ? aiReturn_FAILURE : aiReturn_SUCCESS;
}

static unsigned long assimpFileTell(struct aiFile* file) {
  return lovrFilesystemTell(file->UserData);
}

static struct aiFile* assimpFileOpen(struct aiFileIO* io, const char* path, const char* mode) {
  struct aiFile* file = malloc(sizeof(struct aiFile));
  Blob* blob = (Blob*) io->UserData;
  if (!strcmp(blob->name, path)) {
    file->ReadProc = assimpBlobRead;
    file->FileSizeProc = assimpBlobGetSize;
    file->SeekProc = assimpBlobSeek;
    file->TellProc = assimpBlobTell;
    file->UserData = (void*) blob;
  } else {
    file->ReadProc = assimpFileRead;
    file->FileSizeProc = assimpFileGetSize;
    file->SeekProc = assimpFileSeek;
    file->TellProc = assimpFileTell;
    file->UserData = lovrFilesystemOpen(path, OPEN_READ);
  }
  return file;
}

static void assimpFileClose(struct aiFileIO* io, struct aiFile* file) {
  void* blob = io->UserData;
  if (file->UserData != blob) {
    lovrFilesystemClose(file->UserData);
  }
}

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

  struct aiFileIO assimpIO;
  assimpIO.OpenProc = assimpFileOpen;
  assimpIO.CloseProc = assimpFileClose;
  assimpIO.UserData = (void*) blob;

  struct aiPropertyStore* propertyStore = aiCreatePropertyStore();
  aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
  const struct aiScene* scene = aiImportFileExWithProperties(blob->name, flags, &assimpIO, propertyStore);
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
    modelData->primitives[m].material = assimpMesh->mMaterialIndex;
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

  // Materials
  modelData->materialCount = scene->mNumMaterials;
  modelData->materials = malloc(modelData->materialCount * sizeof(MaterialData));
  for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
    MaterialData* materialData = lovrMaterialDataCreateEmpty();
    struct aiMaterial* material = scene->mMaterials[m];
    struct aiColor4D color;
    struct aiString str;

    if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == aiReturn_SUCCESS) {
      materialData->colors[COLOR_DIFFUSE].r = color.r * 255;
      materialData->colors[COLOR_DIFFUSE].g = color.g * 255;
      materialData->colors[COLOR_DIFFUSE].b = color.b * 255;
      materialData->colors[COLOR_DIFFUSE].a = color.a * 255;
    }

    if (aiGetMaterialTexture(material, aiTextureType_DIFFUSE, 0, &str, NULL, NULL, NULL, NULL, NULL, NULL) == aiReturn_SUCCESS) {
      char* path = str.data;

      if (strstr(path, "./") == path) {
        path += 2;
      } else if (path[0] == '/') {
        lovrThrow("Absolute paths are not supported");
      }

      char fullpath[LOVR_PATH_MAX];
      char* dir = dirname((char*) blob->name);
      snprintf(fullpath, LOVR_PATH_MAX, "%s/%s", dir, path);

      size_t size;
      void* data = lovrFilesystemRead(fullpath, &size);
      if (data) {
        Blob* blob = lovrBlobCreate(data, size, path);
        materialData->textures[TEXTURE_DIFFUSE] = lovrTextureDataFromBlob(blob);
      }
    }

    modelData->materials[m] = *materialData;
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

  for (int i = 0; i < modelData->materialCount; i++) {
    lovrMaterialDataDestroy(&modelData->materials[i]);
  }

  free(modelData->nodes);
  free(modelData->primitives);
  free(modelData->materials);
  free(modelData->vertices);
  free(modelData->indices);
  free(modelData);
}
