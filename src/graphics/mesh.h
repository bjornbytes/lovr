#include "glfw.h"
#include "util.h"
#include "graphics/texture.h"
#include "math/math.h"

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
  MESH_BYTE = GL_BYTE,
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
  int size;
  int stride;
  void* data;
  void* scratchVertex;
  MeshFormat format;
  MeshDrawMode drawMode;
  MeshUsage usage;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  vec_uint_t map;
  char isRangeEnabled;
  int rangeStart;
  int rangeCount;
  Texture* texture;
} Mesh;

Mesh* lovrMeshCreate(int size, MeshFormat* format, MeshDrawMode drawMode, MeshUsage usage);
void lovrMeshDestroy(const Ref* ref);
void lovrMeshDraw(Mesh* mesh, mat4 transform);
MeshFormat lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
int lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
int lovrMeshGetVertexSize(Mesh* mesh);
void* lovrMeshGetScratchVertex(Mesh* mesh);
void lovrMeshGetVertex(Mesh* mesh, int index, void* dest);
void lovrMeshSetVertex(Mesh* mesh, int index, void* vertex);
void lovrMeshSetVertices(Mesh* mesh, void* vertices, int size);
unsigned int* lovrMeshGetVertexMap(Mesh* mesh, int* count);
void lovrMeshSetVertexMap(Mesh* mesh, unsigned int* map, int count);
char lovrMeshIsRangeEnabled(Mesh* mesh);
void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled);
void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count);
int lovrMeshSetDrawRange(Mesh* mesh, int start, int count);
Texture* lovrMeshGetTexture(Mesh* mesh);
void lovrMeshSetTexture(Mesh* mesh, Texture* texture);
