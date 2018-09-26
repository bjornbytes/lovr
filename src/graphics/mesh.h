#include "data/modelData.h"
#include "graphics/material.h"
#include "graphics/shader.h"
#include "graphics/opengl.h"
#include "data/vertexData.h"
#include "lib/map/map.h"
#include <stdbool.h>

#pragma once

#define MAX_ATTRIBUTES 16

typedef struct {
  Buffer* buffer;
  size_t offset;
  size_t stride;
  AttributeType type;
  uint8_t components;
  uint8_t divisor;
  bool integer;
  bool enabled;
} MeshAttribute;

typedef map_t(MeshAttribute) map_attribute_t;

typedef struct {
  Ref ref;
  DrawMode mode;
  VertexFormat format;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  uint32_t vertexCount;
  uint32_t indexCount;
  size_t indexSize;
  uint32_t drawStart;
  uint32_t drawCount;
  Material* material;
  map_attribute_t attributes;
  MeshAttribute layout[MAX_ATTRIBUTES];
  uint16_t divisors[MAX_ATTRIBUTES];
  GPU_MESH_FIELDS
} Mesh;

Mesh* lovrMeshInit(Mesh* mesh, DrawMode mode, VertexFormat format, Buffer* vertexBuffer, uint32_t vertexCount);
Mesh* lovrMeshInitEmpty(Mesh* mesh, DrawMode drawMode);
#define lovrMeshCreate(...) lovrMeshInit(lovrAlloc(Mesh), __VA_ARGS__)
#define lovrMeshCreateEmpty(...) lovrMeshInitEmpty(lovrAlloc(Mesh), __VA_ARGS__)
void lovrMeshDestroy(void* ref);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
Buffer* lovrMeshGetVertexBuffer(Mesh* mesh);
Buffer* lovrMeshGetIndexBuffer(Mesh* mesh);
void lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer, uint32_t indexCount, size_t indexSize);
uint32_t lovrMeshGetVertexCount(Mesh* mesh);
uint32_t lovrMeshGetIndexCount(Mesh* mesh);
size_t lovrMeshGetIndexSize(Mesh* mesh);
void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
DrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count);
void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
