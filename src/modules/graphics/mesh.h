#include "data/modelData.h"
#include <stdbool.h>

#pragma once

#define MAX_ATTRIBUTES 16
#define MAX_ATTRIBUTE_NAME_LENGTH 32

struct Buffer;
struct Material;

typedef struct {
  struct Buffer* buffer;
  uint32_t offset;
  unsigned stride : 8;
  unsigned divisor : 8;
  unsigned type : 3; // AttributeType
  unsigned components : 3;
  unsigned normalized : 1;
  unsigned integer : 1;
  unsigned disabled : 1;
} MeshAttribute;

typedef struct Mesh Mesh;
Mesh* lovrMeshCreate(DrawMode mode, struct Buffer* vertexBuffer, uint32_t vertexCount);
void lovrMeshDestroy(void* ref);
struct Buffer* lovrMeshGetVertexBuffer(Mesh* mesh);
struct Buffer* lovrMeshGetIndexBuffer(Mesh* mesh);
void lovrMeshSetIndexBuffer(Mesh* mesh, struct Buffer* buffer, uint32_t indexCount, size_t indexSize, size_t offset);
uint32_t lovrMeshGetVertexCount(Mesh* mesh);
uint32_t lovrMeshGetIndexCount(Mesh* mesh);
size_t lovrMeshGetIndexSize(Mesh* mesh);
uint32_t lovrMeshGetAttributeCount(Mesh* mesh);
void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
const MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, uint32_t index);
uint32_t lovrMeshGetAttributeIndex(Mesh* mesh, const char* name);
const char* lovrMeshGetAttributeName(Mesh* mesh, uint32_t index);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
DrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count);
void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count);
struct Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, struct Material* material);
