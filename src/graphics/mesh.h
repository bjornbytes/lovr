#include "util.h"
#include "graphics/shader.h"
#include "graphics/material.h"
#include "math/math.h"
#include "lib/glfw.h"
#include "lib/vertex.h"

#pragma once

typedef enum {
  MESH_POINTS = GL_POINTS,
  MESH_LINES = GL_LINES,
  MESH_LINE_STRIP = GL_LINE_STRIP,
  MESH_TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
  MESH_TRIANGLES = GL_TRIANGLES,
  MESH_TRIANGLE_FAN = GL_TRIANGLE_FAN
} MeshDrawMode;

typedef enum {
  MESH_STATIC = GL_STATIC_DRAW,
  MESH_DYNAMIC = GL_DYNAMIC_DRAW,
  MESH_STREAM = GL_STREAM_DRAW
} MeshUsage;

typedef struct {
  Ref ref;
  size_t vertexCount;
  VertexFormat format;
  VertexData vertices;
  IndexData indices;
  size_t indexCount;
  size_t indexSize;
  int enabledAttributes;
  bool attributesDirty;
  bool isMapped;
  int mapStart;
  size_t mapCount;
  bool isRangeEnabled;
  int rangeStart;
  int rangeCount;
  MeshDrawMode drawMode;
  MeshUsage usage;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  Material* material;
  Shader* lastShader;
} Mesh;

Mesh* lovrMeshCreate(size_t count, VertexFormat* format, MeshDrawMode drawMode, MeshUsage usage);
void lovrMeshDestroy(const Ref* ref);
void lovrMeshDraw(Mesh* mesh, mat4 transform, float* pose, int instances);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
IndexData lovrMeshGetVertexMap(Mesh* mesh, size_t* count);
void lovrMeshSetVertexMap(Mesh* mesh, void* data, size_t count);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
bool lovrMeshIsRangeEnabled(Mesh* mesh);
void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled);
void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count);
void lovrMeshSetDrawRange(Mesh* mesh, int start, int count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
VertexData lovrMeshMap(Mesh* mesh, int start, size_t count, bool read, bool write);
void lovrMeshUnmap(Mesh* mesh);
