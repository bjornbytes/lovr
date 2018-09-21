#include "data/modelData.h"
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

#ifdef LOVR_USE_ASSIMP
#include <assimp/cfileio.h>
#include <assimp/cimport.h>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/postprocess.h>

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
  if (node->parent == -1) {
    mat4_set(node->globalTransform, node->transform);
  } else {
    mat4_set(node->globalTransform, modelData->nodes[node->parent].globalTransform);
    mat4_multiply(node->globalTransform, node->transform);
  }

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

static void aabbIterator(ModelData* modelData, ModelNode* node, float aabb[6]) {
  for (int i = 0; i < node->primitives.length; i++) {
    ModelPrimitive* primitive = &modelData->primitives[node->primitives.data[i]];
    for (int j = 0; j < primitive->drawCount; j++) {
      uint32_t index;
      if (modelData->indexSize == sizeof(uint16_t)) {
        index = modelData->indices.shorts[primitive->drawStart + j];
      } else {
        index = modelData->indices.ints[primitive->drawStart + j];
      }
      float vertex[3];
      VertexPointer vertices = { .raw = modelData->vertexData->blob.data };
      vec3_init(vertex, (float*) (vertices.bytes + index * modelData->vertexData->format.stride));
      mat4_transform(node->globalTransform, &vertex[0], &vertex[1], &vertex[2]);
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
    aabbIterator(modelData, child, aabb);
  }
}

static float readMaterialScalar(struct aiMaterial* assimpMaterial, const char* key, unsigned int type, unsigned int index) {
  float scalar;
  if (aiGetMaterialFloatArray(assimpMaterial, key, type, index, &scalar, NULL) == aiReturn_SUCCESS) {
    return scalar;
  } else {
    return 1.f;
  }
}

static Color readMaterialColor(struct aiMaterial* assimpMaterial, const char* key, unsigned int type, unsigned int index) {
  struct aiColor4D assimpColor;
  if (aiGetMaterialColor(assimpMaterial, key, type, index, &assimpColor) == aiReturn_SUCCESS) {
    return (Color) {
      .r = assimpColor.r,
      .g = assimpColor.g,
      .b = assimpColor.b,
      .a = assimpColor.a
    };
  } else {
    return (Color) { 1, 1, 1, 1 };
  }
}

static int readMaterialTexture(struct aiMaterial* assimpMaterial, enum aiTextureType type, ModelData* modelData, map_int_t* textureCache, const char* dirname) {
  struct aiString str;

  if (aiGetMaterialTexture(assimpMaterial, type, 0, &str, NULL, NULL, NULL, NULL, NULL, NULL) != aiReturn_SUCCESS) {
    return 0;
  }

  char* path = str.data;

  int* cachedTexture = map_get(textureCache, path);
  if (cachedTexture) {
    return *cachedTexture;
  }

  char fullPath[LOVR_PATH_MAX];
  char normalizedPath[LOVR_PATH_MAX];
  strncpy(fullPath, dirname, LOVR_PATH_MAX);
  char* lastSlash = strrchr(fullPath, '/');
  if (lastSlash) lastSlash[1] = '\0';
  else fullPath[0] = '\0';
  strncat(fullPath, path, LOVR_PATH_MAX);
  normalizePath(fullPath, normalizedPath, LOVR_PATH_MAX);

  size_t size;
  void* data = lovrFilesystemRead(normalizedPath, &size);
  if (!data) {
    return 0;
  }

  Blob* blob = lovrBlobCreate(data, size, path);
  TextureData* textureData = lovrTextureDataCreateFromBlob(blob, true);
  lovrRelease(blob);
  int textureIndex = modelData->textures.length;
  vec_push(&modelData->textures, textureData);
  map_set(textureCache, path, textureIndex);
  return textureIndex;
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

static size_t assimpFileTell(struct aiFile* assimpFile) {
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
      lovrRelease(file);
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
    lovrRelease(file);
  }
  free(assimpFile);
}

ModelData* lovrModelDataCreate(Blob* blob) {
  ModelData* modelData = lovrAlloc(ModelData, lovrModelDataDestroy);
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

  uint32_t vertexCount = 0;
  bool hasNormals = false;
  bool hasUVs = false;
  bool hasVertexColors = false;
  bool hasTangents = false;
  bool isSkinned = false;

  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    vertexCount += assimpMesh->mNumVertices;
    modelData->indexCount += assimpMesh->mNumFaces * 3;
    hasNormals |= assimpMesh->mNormals != NULL;
    hasUVs |= assimpMesh->mTextureCoords[0] != NULL;
    hasVertexColors |= assimpMesh->mColors[0] != NULL;
    hasTangents |= assimpMesh->mTangents != NULL;
    isSkinned |= assimpMesh->mNumBones > 0;
  }

  VertexFormat format;
  vertexFormatInit(&format);
  vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);

  if (hasNormals) vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
  if (hasUVs) vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);
  if (hasVertexColors) vertexFormatAppend(&format, "lovrVertexColor", ATTR_BYTE, 4);
  if (hasTangents) vertexFormatAppend(&format, "lovrTangent", ATTR_FLOAT, 3);
  size_t boneByteOffset = format.stride;
  if (isSkinned) vertexFormatAppend(&format, "lovrBones", ATTR_INT, 4);
  if (isSkinned) vertexFormatAppend(&format, "lovrBoneWeights", ATTR_FLOAT, 4);

  // Allocate
  modelData->vertexData = lovrVertexDataCreate(vertexCount, &format);
  modelData->indexSize = vertexCount > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
  modelData->indices.raw = malloc(modelData->indexCount * modelData->indexSize);
  modelData->primitiveCount = scene->mNumMeshes;
  modelData->primitives = malloc(modelData->primitiveCount * sizeof(ModelPrimitive));

  // Load vertices
  IndexPointer indices = modelData->indices;
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
      VertexPointer vertices = { .raw = modelData->vertexData->blob.data };
      vertices.bytes += vertex * modelData->vertexData->format.stride;

      *vertices.floats++ = assimpMesh->mVertices[v].x;
      *vertices.floats++ = assimpMesh->mVertices[v].y;
      *vertices.floats++ = assimpMesh->mVertices[v].z;

      if (hasNormals) {
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

      if (hasUVs) {
        if (assimpMesh->mTextureCoords[0]) {
          *vertices.floats++ = assimpMesh->mTextureCoords[0][v].x;
          *vertices.floats++ = assimpMesh->mTextureCoords[0][v].y;
        } else {
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
        }
      }

      if (hasVertexColors) {
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

      if (hasTangents) {
        if (assimpMesh->mTangents) {
          *vertices.floats++ = assimpMesh->mTangents[v].x;
          *vertices.floats++ = assimpMesh->mTangents[v].y;
          *vertices.floats++ = assimpMesh->mTangents[v].z;
        } else {
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
          *vertices.floats++ = 0;
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
        VertexPointer vertices = { .raw = modelData->vertexData->blob.data };
        vertices.bytes += vertexIndex * modelData->vertexData->format.stride;
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
  map_int_t textureCache;
  map_init(&textureCache);
  vec_init(&modelData->textures);
  vec_push(&modelData->textures, NULL);
  modelData->materialCount = scene->mNumMaterials;
  modelData->materials = malloc(modelData->materialCount * sizeof(ModelMaterial));
  for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
    ModelMaterial* material = &modelData->materials[m];
    struct aiMaterial* assimpMaterial = scene->mMaterials[m];

    material->diffuseColor = readMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE);
    material->emissiveColor = readMaterialColor(assimpMaterial, AI_MATKEY_COLOR_EMISSIVE);
    material->diffuseTexture = readMaterialTexture(assimpMaterial, aiTextureType_DIFFUSE, modelData, &textureCache, blob->name);
    material->emissiveTexture = readMaterialTexture(assimpMaterial, aiTextureType_EMISSIVE, modelData, &textureCache, blob->name);
    material->metalnessTexture = readMaterialTexture(assimpMaterial, aiTextureType_UNKNOWN, modelData, &textureCache, blob->name);
    material->roughnessTexture = material->metalnessTexture;
    material->occlusionTexture = readMaterialTexture(assimpMaterial, aiTextureType_LIGHTMAP, modelData, &textureCache, blob->name);
    material->normalTexture = readMaterialTexture(assimpMaterial, aiTextureType_NORMALS, modelData, &textureCache, blob->name);
    material->metalness = readMaterialScalar(assimpMaterial, "$mat.gltf.pbrMetallicRoughness.metallicFactor", 0, 0);
    material->roughness = readMaterialScalar(assimpMaterial, "$mat.gltf.pbrMetallicRoughness.roughnessFactor", 0, 0);
  }
  map_deinit(&textureCache);

  // Nodes
  modelData->nodeCount = 0;
  assimpSumChildren(scene->mRootNode, &modelData->nodeCount);
  modelData->nodes = malloc(modelData->nodeCount * sizeof(ModelNode));
  modelData->nodes[0].parent = -1;
  map_init(&modelData->nodeMap);
  int nodeIndex = 0;
  assimpNodeTraversal(modelData, scene->mRootNode, &nodeIndex);

  // Animations
  modelData->animationCount = scene->mNumAnimations;
  modelData->animations = malloc(modelData->animationCount * sizeof(Animation));
  for (int i = 0; i < modelData->animationCount; i++) {
    struct aiAnimation* assimpAnimation = scene->mAnimations[i];
    float ticksPerSecond = assimpAnimation->mTicksPerSecond;

    Animation* animation = &modelData->animations[i];
    animation->name = strdup(assimpAnimation->mName.data);
    animation->duration = assimpAnimation->mDuration / ticksPerSecond;
    animation->channelCount = assimpAnimation->mNumChannels;
    map_init(&animation->channels);

    for (int j = 0; j < animation->channelCount; j++) {
      struct aiNodeAnim* assimpChannel = assimpAnimation->mChannels[j];
      AnimationChannel channel;

      channel.node = strdup(assimpChannel->mNodeName.data);
      vec_init(&channel.positionKeyframes);
      vec_init(&channel.rotationKeyframes);
      vec_init(&channel.scaleKeyframes);

      for (unsigned int k = 0; k < assimpChannel->mNumPositionKeys; k++) {
        struct aiVectorKey assimpKeyframe = assimpChannel->mPositionKeys[k];
        struct aiVector3D position = assimpKeyframe.mValue;
        vec_push(&channel.positionKeyframes, ((Keyframe) {
          .time = assimpKeyframe.mTime / ticksPerSecond,
          .data = { position.x, position.y, position.z }
        }));
      }

      for (unsigned int k = 0; k < assimpChannel->mNumRotationKeys; k++) {
        struct aiQuatKey assimpKeyframe = assimpChannel->mRotationKeys[k];
        struct aiQuaternion quaternion = assimpKeyframe.mValue;
        vec_push(&channel.rotationKeyframes, ((Keyframe) {
          .time = assimpKeyframe.mTime / ticksPerSecond,
          .data = { quaternion.x, quaternion.y, quaternion.z, quaternion.w }
        }));
      }

      for (unsigned int k = 0; k < assimpChannel->mNumScalingKeys; k++) {
        struct aiVectorKey assimpKeyframe = assimpChannel->mScalingKeys[k];
        struct aiVector3D scale = assimpKeyframe.mValue;
        vec_push(&channel.scaleKeyframes, ((Keyframe) {
          .time = assimpKeyframe.mTime / ticksPerSecond,
          .data = { scale.x, scale.y, scale.z }
        }));
      }

      map_set(&animation->channels, channel.node, channel);
    }
  }

  aiReleaseImport(scene);
  return modelData;
}
#else
static void aabbIterator(ModelData* modelData, ModelNode* node, float aabb[6]) {}
ModelData* lovrModelDataCreate(Blob* blob) {
  return NULL;
}
#endif

ModelData* lovrModelDataCreateEmpty() {
  return lovrAlloc(ModelData, lovrModelDataDestroy);
}

void lovrModelDataDestroy(void* ref) {
  ModelData* modelData = ref;

  for (int i = 0; i < modelData->nodeCount; i++) {
    vec_deinit(&modelData->nodes[i].children);
    vec_deinit(&modelData->nodes[i].primitives);
    free((char*) modelData->nodes[i].name);
  }

  for (int i = 0; i < modelData->primitiveCount; i++) {
    ModelPrimitive* primitive = &modelData->primitives[i];
    for (int j = 0; j < primitive->boneCount; j++) {
      free((char*) primitive->bones[j].name);
    }
    map_deinit(&primitive->boneMap);
  }

  for (int i = 0; i < modelData->animationCount; i++) {
    Animation* animation = &modelData->animations[i];
    const char* key;
    map_iter_t iter = map_iter(&animation->channels);
    while ((key = map_next(&animation->channels, &iter)) != NULL) {
      AnimationChannel* channel = map_get(&animation->channels, key);
      vec_deinit(&channel->positionKeyframes);
      vec_deinit(&channel->rotationKeyframes);
      vec_deinit(&channel->scaleKeyframes);
    }
    map_deinit(&animation->channels);
    free((char*) animation->name);
  }

  for (int i = 0; i < modelData->textures.length; i++) {
    lovrRelease(modelData->textures.data[i]);
  }

  vec_deinit(&modelData->textures);
  map_deinit(&modelData->nodeMap);

  lovrRelease(modelData->vertexData);

  free(modelData->nodes);
  free(modelData->primitives);
  free(modelData->animations);
  free(modelData->materials);
  free(modelData->indices.raw);
  free(modelData);
}

void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]) {
  aabb[0] = FLT_MAX;
  aabb[1] = -FLT_MAX;
  aabb[2] = FLT_MAX;
  aabb[3] = -FLT_MAX;
  aabb[4] = FLT_MAX;
  aabb[5] = -FLT_MAX;
  aabbIterator(modelData, &modelData->nodes[0], aabb);
}
