#include "filesystem/blob.h"
#include "data/animation.h"
#include "data/material.h"
#include "util.h"
#include "lib/vec/vec.h"

#pragma once

#define MAX_BONES_PER_VERTEX 4
#define MAX_BONES 48

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
  const char* name;
  float offset[16];
} Bone;

typedef struct {
  int material;
  int drawStart;
  int drawCount;
  Bone bones[MAX_BONES];
  map_int_t boneMap;
  int boneCount;
} ModelPrimitive;

typedef struct ModelNode {
  const char* name;
  float transform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  Ref ref;
  ModelNode* nodes;
  map_int_t nodeMap;
  ModelPrimitive* primitives;
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
  bool skinned;
  size_t stride;
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(const Ref* ref);
void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]);
