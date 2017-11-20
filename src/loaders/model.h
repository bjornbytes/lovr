#include "filesystem/blob.h"
#include "loaders/animation.h"
#include "loaders/material.h"
#include "util.h"
#include "lib/vec/vec.h"

#pragma once

#define MAX_BONES_PER_VERTEX 4

typedef union {
  void* data;
  uint8_t* bytes;
  uint32_t* ints;
  float* floats;
} ModelVertices;

typedef union {
  void* data;
  uint16_t* shorts;
  uint32_t* ints;
} ModelIndices;

typedef struct {
  int material;
  int drawStart;
  int drawCount;
} ModelPrimitive;

typedef struct ModelNode {
  const char* name;
  float transform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  const char* name;
  int nodeIndex;
  float offset[16];
} Bone;

typedef vec_t(Bone) vec_bone_t;

typedef struct {
  ModelNode* nodes;
  ModelPrimitive* primitives;
  vec_bone_t bones;
  map_int_t boneMap;
  AnimationData* animationData;
  MaterialData** materials;
  ModelVertices vertices;
  ModelIndices indices;
  int nodeCount;
  int primitiveCount;
  int animationCount;
  int materialCount;
  int vertexCount;
  int indexCount;
  size_t indexSize;
  bool hasNormals;
  bool hasUVs;
  bool hasVertexColors;
  bool hasBones;
  size_t boneByteOffset;
  size_t stride;
  float inverseRootTransform[16];
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(ModelData* modelData);
void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]);
void lovrModelDataGetCenter(ModelData* modelData, float center[3]);
