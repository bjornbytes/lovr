#include "data/modelData.h"
#include "graphics/material.h"
#include "graphics/shader.h"
#include "graphics/opengl.h"
#include "lib/map/map.h"
#include <stdbool.h>

#pragma once

#define MAX_ATTRIBUTES 16

typedef struct {
  Buffer* buffer;
  uint32_t offset;
  unsigned stride : 8;
  unsigned divisor : 8;
  unsigned type : 3; // AttributeType
  unsigned components : 3;
  unsigned normalized : 1;
  unsigned integer : 1;
  unsigned disabled : 1;
} MeshAttribute;

typedef struct {
  Ref ref;
  DrawMode mode;
  const char* attributeNames[MAX_ATTRIBUTES];
  MeshAttribute attributes[MAX_ATTRIBUTES];
  uint8_t locations[MAX_ATTRIBUTES];
  uint16_t enabledLocations;
  uint16_t divisors[MAX_ATTRIBUTES];
  map_int_t attributeMap;
  int attributeCount;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  uint32_t vertexCount;
  uint32_t indexCount;
  size_t indexSize;
  size_t indexOffset;
  uint32_t drawStart;
  uint32_t drawCount;
  Material* material;
  GPU_MESH_FIELDS
} Mesh;

Mesh* lovrMeshInit(Mesh* mesh, DrawMode mode, Buffer* vertexBuffer, uint32_t vertexCount);
#define lovrMeshCreate(...) lovrMeshInit(lovrAlloc(Mesh), __VA_ARGS__)
void lovrMeshDestroy(void* ref);
Buffer* lovrMeshGetVertexBuffer(Mesh* mesh);
Buffer* lovrMeshGetIndexBuffer(Mesh* mesh);
void lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer, uint32_t indexCount, size_t indexSize, size_t offset);
uint32_t lovrMeshGetVertexCount(Mesh* mesh);
uint32_t lovrMeshGetIndexCount(Mesh* mesh);
size_t lovrMeshGetIndexSize(Mesh* mesh);
void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
const MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
DrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count);
void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
