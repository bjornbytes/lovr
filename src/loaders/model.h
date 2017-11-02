#include "filesystem/blob.h"
#include "loaders/material.h"
#include "util.h"
#include "lib/vec/vec.h"

#pragma once

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
  float transform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  ModelNode* nodes;
  ModelPrimitive* primitives;
  MaterialData* materials;
  ModelVertices vertices;
  ModelIndices indices;
  int nodeCount;
  int primitiveCount;
  int materialCount;
  int vertexCount;
  int indexCount;
  size_t indexSize;
  bool hasNormals;
  bool hasUVs;
  bool hasVertexColors;
  size_t stride;
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(ModelData* modelData);
void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]);
void lovrModelDataGetCenter(ModelData* modelData, float center[3]);
