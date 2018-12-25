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

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_LINE_STRIP,
  DRAW_LINE_LOOP,
  DRAW_TRIANGLE_STRIP,
  DRAW_TRIANGLES,
  DRAW_TRIANGLE_FAN
} DrawMode;

typedef struct {
  Ref ref;
  uint32_t count;
  DrawMode mode;
  VertexFormat format;
  bool readable;
  bool dirty;
  BufferUsage usage;
  Buffer* vbo;
  Buffer* ibo;
  uint32_t indexCount;
  size_t indexSize;
  size_t indexCapacity;
  uint32_t rangeStart;
  uint32_t rangeCount;
  Material* material;
  map_attribute_t attributes;
  MeshAttribute layout[MAX_ATTRIBUTES];
  GPU_MESH_FIELDS
} Mesh;

Mesh* lovrMeshInit(Mesh* mesh, uint32_t count, VertexFormat format, DrawMode drawMode, BufferUsage usage, bool readable);
#define lovrMeshCreate(...) lovrMeshInit(lovrAlloc(Mesh), __VA_ARGS__)
void lovrMeshDestroy(void* ref);
void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name);
void lovrMeshBind(Mesh* mesh, Shader* shader, int divisorMultiplier);
bool lovrMeshIsDirty(Mesh* mesh);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
bool lovrMeshIsReadable(Mesh* mesh);
DrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode);
int lovrMeshGetVertexCount(Mesh* mesh);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count);
void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
void* lovrMeshMapVertices(Mesh* mesh, size_t offset);
void lovrMeshFlushVertices(Mesh* mesh, size_t offset, size_t size);
void* lovrMeshMapIndices(Mesh* mesh, uint32_t count, size_t indexSize, size_t offset);
void lovrMeshFlushIndices(Mesh* mesh);
void* lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* indexSize);
