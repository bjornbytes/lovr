#include "util.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/glfw.h"

#pragma once

typedef enum {
  MESH_POINTS = GL_POINTS,
  MESH_TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
  MESH_TRIANGLES = GL_TRIANGLES,
  MESH_TRIANGLE_FAN = GL_TRIANGLE_FAN
} MeshDrawMode;

typedef enum {
  MESH_STATIC = GL_STATIC_DRAW,
  MESH_DYNAMIC = GL_DYNAMIC_DRAW,
  MESH_STREAM = GL_STREAM_DRAW
} MeshUsage;

typedef enum {
  MESH_FLOAT = GL_FLOAT,
  MESH_BYTE = GL_UNSIGNED_BYTE,
  MESH_INT = GL_INT
} MeshAttributeType;

typedef struct {
  const char* name;
  MeshAttributeType type;
  int count;
} MeshAttribute;

typedef vec_t(MeshAttribute) MeshFormat;

typedef struct {
  Ref ref;
  void* data;
  size_t count;
  int stride;
  int enabledAttributes;
  bool attributesDirty;
  bool isMapped;
  int mapStart;
  size_t mapCount;
  MeshFormat format;
  MeshDrawMode drawMode;
  MeshUsage usage;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  void* indices;
  size_t indexCount;
  size_t indexSize;
  bool isRangeEnabled;
  int rangeStart;
  int rangeCount;
  Shader* lastShader;
} Mesh;

Mesh* lovrMeshCreate(size_t count, MeshFormat* format, MeshDrawMode drawMode, MeshUsage usage);
void lovrMeshDestroy(const Ref* ref);
void lovrMeshDraw(Mesh* mesh, mat4 transform, float* pose);
MeshFormat lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
int lovrMeshGetVertexSize(Mesh* mesh);
void* lovrMeshGetVertexMap(Mesh* mesh, size_t* count);
void lovrMeshSetVertexMap(Mesh* mesh, void* data, size_t count);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
bool lovrMeshIsRangeEnabled(Mesh* mesh);
void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled);
void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count);
void lovrMeshSetDrawRange(Mesh* mesh, int start, int count);
Texture* lovrMeshGetTexture(Mesh* mesh);
void lovrMeshSetTexture(Mesh* mesh, Texture* texture);
void* lovrMeshMap(Mesh* mesh, int start, size_t count, bool read, bool write);
void lovrMeshUnmap(Mesh* mesh);
