#include "data/model.h"
#include "filesystem/filesystem.h"
#include "filesystem/file.h"
#include "math/math.h"
#include "math/mat4.h"
#include "math/quat.h"
#include "math/vec3.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <assimp/cfileio.h>
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
  node->name = strdup(assimpNode->mName.data);
  map_set(&modelData->nodeMap, node->name, currentIndex);

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

static void normalizePath(char* path, char* dst, size_t size) {
  char* slash = path;
  while ((slash = strchr(path, '\\')) != NULL) { *slash++ = '/'; }

  if (path[0] == '/') {
    strncpy(dst, path, size);
    return;
  }

  memset(dst, 0, size);

  while (*path != '\0') {
    if (*path == '/') {
      path++;
      continue;
    }
    if (*path == '.') {
      if (path[1] == '\0' || path[1] == '/') {
        path++;
        continue;
      }
      if (path[1] == '.' && (path[2] == '\0' || path[2] == '/')) {
        path += 2;
        while ((--dst)[-1] != '/');
        continue;
      }
    }
    while (*path != '\0' && *path != '/') {
      *dst++ = *path++;
    }
    *dst++ = '/';
  }

  *--dst = '\0';
}

// Blob IO (to avoid reading data twice)
static size_t assimpBlobRead(struct aiFile* assimpFile, char* buffer, size_t size, size_t count) {
  Blob* blob = (Blob*) assimpFile->UserData;
  char* data = blob->data;
  size_t bytes = MIN(count * size * sizeof(char), blob->size - blob->seek);
  memcpy(buffer, data + blob->seek, bytes);
  blob->seek += bytes;
  return bytes / size;
}

static size_t assimpBlobGetSize(struct aiFile* assimpFile) {
  Blob* blob = (Blob*) assimpFile->UserData;
  return blob->size;
}

static aiReturn assimpBlobSeek(struct aiFile* assimpFile, size_t position, enum aiOrigin origin) {
  Blob* blob = (Blob*) assimpFile->UserData;
  switch (origin) {
    case aiOrigin_SET: blob->seek = position; break;
    case aiOrigin_CUR: blob->seek += position; break;
    case aiOrigin_END: blob->seek = blob->size - position; break;
    default: return aiReturn_FAILURE;
  }
  return blob->seek < blob->size ? aiReturn_SUCCESS : aiReturn_FAILURE;
}

static size_t assimpBlobTell(struct aiFile* assimpFile) {
  Blob* blob = (Blob*) assimpFile->UserData;
  return blob->seek;
}

// File IO (for reading referenced materials/textures)
static size_t assimpFileRead(struct aiFile* assimpFile, char* buffer, size_t size, size_t count) {
  File* file = (File*) assimpFile->UserData;
  unsigned long bytes = lovrFileRead(file, buffer, size * count);
  return bytes / size;
}

static size_t assimpFileGetSize(struct aiFile* assimpFile) {
  File* file = (File*) assimpFile->UserData;
  return lovrFileGetSize(file);
}

static aiReturn assimpFileSeek(struct aiFile* assimpFile, size_t position, enum aiOrigin origin) {
  File* file = (File*) assimpFile->UserData;
  return lovrFileSeek(file, position) ? aiReturn_FAILURE : aiReturn_SUCCESS;
}

static unsigned long assimpFileTell(struct aiFile* assimpFile) {
  File* file = (File*) assimpFile->UserData;
  return lovrFileTell(file);
}

static struct aiFile* assimpFileOpen(struct aiFileIO* io, const char* path, const char* mode) {
  struct aiFile* assimpFile = malloc(sizeof(struct aiFile));
  Blob* blob = (Blob*) io->UserData;
  if (!strcmp(blob->name, path)) {
    blob->seek = 0;
    assimpFile->ReadProc = assimpBlobRead;
    assimpFile->FileSizeProc = assimpBlobGetSize;
    assimpFile->SeekProc = assimpBlobSeek;
    assimpFile->TellProc = assimpBlobTell;
    assimpFile->UserData = (void*) blob;
  } else {
    char tempPath[LOVR_PATH_MAX];
    char normalizedPath[LOVR_PATH_MAX];
    strncpy(tempPath, path, LOVR_PATH_MAX);
    normalizePath(tempPath, normalizedPath, LOVR_PATH_MAX);

    File* file = lovrFileCreate(normalizedPath);
    if (lovrFileOpen(file, OPEN_READ)) {
      return NULL;
    }

    assimpFile->ReadProc = assimpFileRead;
    assimpFile->FileSizeProc = assimpFileGetSize;
    assimpFile->SeekProc = assimpFileSeek;
    assimpFile->TellProc = assimpFileTell;
    assimpFile->UserData = (void*) file;
  }

  return assimpFile;
}

static void assimpFileClose(struct aiFileIO* io, struct aiFile* assimpFile) {
  void* blob = io->UserData;
  if (assimpFile->UserData != blob) {
    File* file = (File*) assimpFile->UserData;
    lovrFileClose(file);
    lovrRelease(&file->ref);
  }
  free(assimpFile);
}

ModelData* lovrModelDataCreate(Blob* blob) {
  ModelData* modelData = lovrAlloc(sizeof(ModelData), lovrModelDataDestroy);
  if (!modelData) return NULL;

  struct aiFileIO assimpIO;
  assimpIO.OpenProc = assimpFileOpen;
  assimpIO.CloseProc = assimpFileClose;
  assimpIO.UserData = (void*) blob;

  struct aiPropertyStore* propertyStore = aiCreatePropertyStore();
  aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  aiSetImportPropertyInteger(propertyStore, AI_CONFIG_PP_SBBC_MAX_BONES, 48);
  unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_SplitByBoneCount;
  const struct aiScene* scene = aiImportFileExWithProperties(blob->name, flags, &assimpIO, propertyStore);
  aiReleasePropertyStore(propertyStore);

  if (!scene) {
    lovrThrow("Unable to load model from '%s': %s\n", blob->name, aiGetErrorString());
  }

  modelData->nodeCount = 0;
  modelData->vertexCount = 0;
  modelData->indexCount = 0;
  modelData->hasNormals = false;
  modelData->hasUVs = false;
  modelData->hasVertexColors = false;
  modelData->skinned = false;

  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    modelData->vertexCount += assimpMesh->mNumVertices;
    modelData->indexCount += assimpMesh->mNumFaces * 3;
    modelData->hasNormals |= assimpMesh->mNormals != NULL;
    modelData->hasUVs |= assimpMesh->mTextureCoords[0] != NULL;
    modelData->hasVertexColors |= assimpMesh->mColors[0] != NULL;
    modelData->skinned |= assimpMesh->mNumBones > 0;
  }

  // Allocate
  modelData->primitiveCount = scene->mNumMeshes;
  modelData->primitives = malloc(modelData->primitiveCount * sizeof(ModelPrimitive));
  modelData->indexSize = modelData->vertexCount > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
  modelData->stride = 3 * sizeof(float);
  modelData->stride += (modelData->hasNormals ? 3 : 0) * sizeof(float);
  modelData->stride += (modelData->hasUVs ? 2 : 0) * sizeof(float);
  modelData->stride += (modelData->hasVertexColors ? 4 : 0) * sizeof(uint8_t);
  size_t boneByteOffset = modelData->stride;
  modelData->stride += (modelData->skinned ? (4 * sizeof(uint32_t) + 4 * sizeof(float)) : 0);
  modelData->vertices.data = malloc(modelData->stride * modelData->vertexCount);
  modelData->indices.data = malloc(modelData->indexCount * modelData->indexSize);
  memset(modelData->vertices.data, 0, modelData->stride * modelData->vertexCount);

  // Load vertices
  ModelIndices indices = modelData->indices;
  uint32_t vertex = 0;
  uint32_t index = 0;
  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    ModelPrimitive* primitive = &modelData->primitives[m];
    primitive->material = assimpMesh->mMaterialIndex;
    primitive->drawStart = index;
    primitive->drawCount = 0;
    uint32_t baseVertex = vertex;

    // Indices
    for (unsigned int f = 0; f < assimpMesh->mNumFaces; f++) {
      struct aiFace assimpFace = assimpMesh->mFaces[f];
      lovrAssert(assimpFace.mNumIndices == 3, "Only triangular faces are supported");

      primitive->drawCount += assimpFace.mNumIndices;

      if (modelData->indexSize == sizeof(uint16_t)) {
        for (unsigned int i = 0; i < assimpFace.mNumIndices; i++) {
          indices.shorts[index++] = baseVertex + assimpFace.mIndices[i];
        }
      } else {
        for (unsigned int i = 0; i < assimpFace.mNumIndices; i++) {
          indices.ints[index++] = baseVertex + assimpFace.mIndices[i];
        }
      }
    }

    // Vertices
    for (unsigned int v = 0; v < assimpMesh->mNumVertices; v++) {
      ModelVertices vertices = modelData->vertices;
      vertices.bytes += vertex * modelData->stride;

      *vertices.floats++ = assimpMesh->mVertices[v].x;
      *vertices.floats++ = assimpMesh->mVertices[v].y;
      *vertices.floats++ = assimpMesh->mVertices[v].z;

      if (modelData->hasNormals) {
        if (assimpMesh->mNormals) {
          *vertices.floats++ = assimpMesh->mNormals[v].x;
          *vertices.floats++ = assimpMesh->mNormals[v].y;
          *vertices.floats++ = assimpMesh->mNormals[v].z;
        } else {
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
        }
      }

      if (modelData->hasUVs) {
        if (assimpMesh->mTextureCoords[0]) {
          *vertices.floats++ = assimpMesh->mTextureCoords[0][v].x;
          *vertices.floats++ = assimpMesh->mTextureCoords[0][v].y;
        } else {
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
        }
      }

      if (modelData->hasVertexColors) {
        if (assimpMesh->mColors[0]) {
          *vertices.bytes++ = assimpMesh->mColors[0][v].r * 255;
          *vertices.bytes++ = assimpMesh->mColors[0][v].g * 255;
          *vertices.bytes++ = assimpMesh->mColors[0][v].b * 255;
          *vertices.bytes++ = assimpMesh->mColors[0][v].a * 255;
        } else {
          *vertices.bytes++ = 255;
          *vertices.bytes++ = 255;
          *vertices.bytes++ = 255;
          *vertices.bytes++ = 255;
        }
      }

      vertex++;
    }

    // Bones
    primitive->boneCount = assimpMesh->mNumBones;
    map_init(&primitive->boneMap);
    for (unsigned int b = 0; b < assimpMesh->mNumBones; b++) {
      struct aiBone* assimpBone = assimpMesh->mBones[b];
      Bone* bone = &primitive->bones[b];

      bone->name = strdup(assimpBone->mName.data);
      aiTransposeMatrix4(&assimpBone->mOffsetMatrix);
      mat4_set(bone->offset, (float*) &assimpBone->mOffsetMatrix);
      map_set(&primitive->boneMap, bone->name, b);

      for (unsigned int w = 0; w < assimpBone->mNumWeights; w++) {
        uint32_t vertexIndex = baseVertex + assimpBone->mWeights[w].mVertexId;
        float weight = assimpBone->mWeights[w].mWeight;
        ModelVertices vertices = modelData->vertices;
        vertices.bytes += vertexIndex * modelData->stride;
        uint32_t* bones = (uint32_t*) (vertices.bytes + boneByteOffset);
        float* weights = (float*) (bones + MAX_BONES_PER_VERTEX);

        int boneSlot = 0;
        while (weights[boneSlot] > 0) {
          boneSlot++;
          lovrAssert(boneSlot < MAX_BONES_PER_VERTEX, "Too many bones for vertex %d", vertexIndex);
        }

        bones[boneSlot] = b;
        weights[boneSlot] = weight;
      }
    }
  }

  // Materials
  modelData->materialCount = scene->mNumMaterials;
  modelData->materials = malloc(modelData->materialCount * sizeof(MaterialData*));
  for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
    MaterialData* materialData = lovrMaterialDataCreateEmpty();
    struct aiMaterial* material = scene->mMaterials[m];
    struct aiColor4D color;
    struct aiString str;

    if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == aiReturn_SUCCESS) {
      materialData->colors[COLOR_DIFFUSE].r = color.r;
      materialData->colors[COLOR_DIFFUSE].g = color.g;
      materialData->colors[COLOR_DIFFUSE].b = color.b;
      materialData->colors[COLOR_DIFFUSE].a = color.a;
    }

    if (aiGetMaterialTexture(material, aiTextureType_DIFFUSE, 0, &str, NULL, NULL, NULL, NULL, NULL, NULL) == aiReturn_SUCCESS) {
      char* path = str.data;
      char fullPath[LOVR_PATH_MAX];
      char normalizedPath[LOVR_PATH_MAX];
      strncpy(fullPath, blob->name, LOVR_PATH_MAX);
      char* lastSlash = strrchr(fullPath, '/');
      if (lastSlash) lastSlash[1] = '\0';
      strncat(fullPath, path, LOVR_PATH_MAX);
      normalizePath(fullPath, normalizedPath, LOVR_PATH_MAX);

      size_t size;
      void* data = lovrFilesystemRead(normalizedPath, &size);
      if (data) {
        Blob* blob = lovrBlobCreate(data, size, path);
        materialData->textures[TEXTURE_DIFFUSE] = lovrTextureDataFromBlob(blob);
      }
    }

    modelData->materials[m] = materialData;
  }

  // Nodes
  modelData->nodeCount = 0;
  assimpSumChildren(scene->mRootNode, &modelData->nodeCount);
  modelData->nodes = malloc(modelData->nodeCount * sizeof(ModelNode));
  modelData->nodes[0].parent = -1;
  map_init(&modelData->nodeMap);
  int nodeIndex = 0;
  assimpNodeTraversal(modelData, scene->mRootNode, &nodeIndex);

  // Animations
  modelData->animationData = lovrAnimationDataCreate();
  for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
    struct aiAnimation* assimpAnimation = scene->mAnimations[i];
    float ticksPerSecond = assimpAnimation->mTicksPerSecond;

    Animation animation;
    animation.name = strdup(assimpAnimation->mName.data);
    animation.duration = assimpAnimation->mDuration / ticksPerSecond;
    animation.channelCount = assimpAnimation->mNumChannels;
    map_init(&animation.channels);

    for (int j = 0; j < animation.channelCount; j++) {
      struct aiNodeAnim* assimpChannel = assimpAnimation->mChannels[j];
      AnimationChannel channel;

      channel.node = strdup(assimpChannel->mNodeName.data);
      vec_init(&channel.positionKeyframes);
      vec_init(&channel.rotationKeyframes);
      vec_init(&channel.scaleKeyframes);

      for (unsigned int k = 0; k < assimpChannel->mNumPositionKeys; k++) {
        struct aiVectorKey assimpKeyframe = assimpChannel->mPositionKeys[k];
        struct aiVector3D position = assimpKeyframe.mValue;
        Keyframe keyframe;
        keyframe.time = assimpKeyframe.mTime / ticksPerSecond;
        vec3_set(keyframe.data, position.x, position.y, position.z);
        vec_push(&channel.positionKeyframes, keyframe);
      }

      for (unsigned int k = 0; k < assimpChannel->mNumRotationKeys; k++) {
        struct aiQuatKey assimpKeyframe = assimpChannel->mRotationKeys[k];
        struct aiQuaternion quaternion = assimpKeyframe.mValue;
        Keyframe keyframe;
        keyframe.time = assimpKeyframe.mTime / ticksPerSecond;
        quat_set(keyframe.data, quaternion.x, quaternion.y, quaternion.z, quaternion.w);
        vec_push(&channel.rotationKeyframes, keyframe);
      }

      for (unsigned int k = 0; k < assimpChannel->mNumScalingKeys; k++) {
        struct aiVectorKey assimpKeyframe = assimpChannel->mScalingKeys[k];
        struct aiVector3D scale = assimpKeyframe.mValue;
        Keyframe keyframe;
        keyframe.time = assimpKeyframe.mTime / ticksPerSecond;
        vec3_set(keyframe.data, scale.x, scale.y, scale.z);
        vec_push(&channel.scaleKeyframes, keyframe);
      }

      map_set(&animation.channels, channel.node, channel);
    }

    vec_push(&modelData->animationData->animations, animation);
  }

  aiReleaseImport(scene);
  return modelData;
}

void lovrModelDataDestroy(const Ref* ref) {
  ModelData* modelData = containerof(ref, ModelData);

  for (int i = 0; i < modelData->nodeCount; i++) {
    vec_deinit(&modelData->nodes[i].children);
    vec_deinit(&modelData->nodes[i].primitives);
  }

  for (int i = 0; i < modelData->primitiveCount; i++) {
    map_deinit(&modelData->primitives[i].boneMap);
  }

  if (modelData->animationData) {
    lovrAnimationDataDestroy(modelData->animationData);
  }

  for (int i = 0; i < modelData->materialCount; i++) {
    lovrMaterialDataDestroy(modelData->materials[i]);
  }

  map_deinit(&modelData->nodeMap);

  free(modelData->nodes);
  free(modelData->primitives);
  free(modelData->materials);
  free(modelData->vertices.data);
  free(modelData->indices.data);
  free(modelData);
}

static void aabbIterator(ModelData* modelData, ModelNode* node, float aabb[6], mat4 transform) {
  mat4_multiply(transform, node->transform);

  for (int i = 0; i < node->primitives.length; i++) {
    ModelPrimitive* primitive = &modelData->primitives[node->primitives.data[i]];
    for (int j = 0; j < primitive->drawCount; j++) {
      float vertex[3];
      uint32_t index;
      if (modelData->indexSize == sizeof(uint16_t)) {
        index = modelData->indices.shorts[primitive->drawStart + j];
      } else {
        index = modelData->indices.ints[primitive->drawStart + j];
      }
      vec3_init(vertex, (float*) (modelData->vertices.bytes + index * modelData->stride));
      mat4_transform(transform, vertex);
      aabb[0] = MIN(aabb[0], vertex[0]);
      aabb[1] = MAX(aabb[1], vertex[0]);
      aabb[2] = MIN(aabb[2], vertex[1]);
      aabb[3] = MAX(aabb[3], vertex[1]);
      aabb[4] = MIN(aabb[4], vertex[2]);
      aabb[5] = MAX(aabb[5], vertex[2]);
    }
  }

  for (int i = 0; i < node->children.length; i++) {
    ModelNode* child = &modelData->nodes[node->children.data[i]];
    aabbIterator(modelData, child, aabb, transform);
  }
}

void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]) {
  float transform[16];
  mat4_identity(transform);
  aabb[0] = FLT_MAX;
  aabb[1] = -FLT_MAX;
  aabb[2] = FLT_MAX;
  aabb[3] = -FLT_MAX;
  aabb[4] = FLT_MAX;
  aabb[5] = -FLT_MAX;
  aabbIterator(modelData, &modelData->nodes[0], aabb, transform);
}
