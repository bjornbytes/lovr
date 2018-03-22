#include "graphics/material.h"
#include "data/vertexData.h"
#include "math/math.h"
#include "lib/glfw.h"
#include "lib/map/map.h"
#include "util.h"

#pragma once

#define MAX_ATTACHMENTS 16

typedef enum {
  MESH_POINTS = GL_POINTS,
  MESH_LINES = GL_LINES,
  MESH_LINE_STRIP = GL_LINE_STRIP,
  MESH_LINE_LOOP = GL_LINE_LOOP,
  MESH_TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
  MESH_TRIANGLES = GL_TRIANGLES,
  MESH_TRIANGLE_FAN = GL_TRIANGLE_FAN
} MeshDrawMode;

typedef enum {
  MESH_STATIC = GL_STATIC_DRAW,
  MESH_DYNAMIC = GL_DYNAMIC_DRAW,
  MESH_STREAM = GL_STREAM_DRAW
} MeshUsage;

typedef struct Mesh Mesh;

typedef struct {
  Mesh* mesh;
  int attributeIndex;
  int divisor;
  bool enabled;
} MeshAttachment;

typedef map_t(MeshAttachment) map_attachment_t;

struct Mesh {
  Ref ref;
  uint32_t count;
  VertexFormat format;
  MeshDrawMode drawMode;
  MeshUsage usage;
#ifdef EMSCRIPTEN
  VertexPointer data;
  IndexPointer indices;
#endif
  uint32_t indexCount;
  size_t indexSize;
  size_t indexCapacity;
  bool mappedIndices;
  bool mappedVertices;
  int mapStart;
  size_t mapCount;
  int rangeStart;
  int rangeCount;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  Material* material;
  map_attachment_t attachments;
  MeshAttachment layout[MAX_ATTACHMENTS];
  bool isAttachment;
};

Mesh* lovrMeshCreate(uint32_t count, VertexFormat format, MeshDrawMode drawMode, MeshUsage usage);
void lovrMeshDestroy(void* ref);
void lovrMeshAttachAttribute(Mesh* mesh, Mesh* other, const char* name, int divisor);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
void lovrMeshDraw(Mesh* mesh, mat4 transform, float* pose, int instances);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count);
void lovrMeshSetDrawRange(Mesh* mesh, int start, int count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
VertexPointer lovrMeshMapVertices(Mesh* mesh, uint32_t start, uint32_t count, bool read, bool write);
void lovrMeshUnmapVertices(Mesh* mesh);
IndexPointer lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* size);
IndexPointer lovrMeshWriteIndices(Mesh* mesh, uint32_t count, size_t size);
void lovrMeshUnmapIndices(Mesh* mesh);
void lovrMeshResize(Mesh* mesh, uint32_t count);
