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
  VertexData* vertexData;
  IndexPointer indices;
  size_t indexCount;
  size_t indexSize;
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
  map_attachment_t attachments;
  MeshAttachment layout[16];
  bool isAttachment;
};

Mesh* lovrMeshCreate(VertexData* vertexData, MeshDrawMode drawMode, MeshUsage usage);
void lovrMeshDestroy(void* ref);
void lovrMeshAttachAttribute(Mesh* mesh, Mesh* other, const char* name, int divisor);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
void lovrMeshDraw(Mesh* mesh, mat4 transform, float* pose, int instances);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
IndexPointer lovrMeshGetVertexMap(Mesh* mesh, size_t* count);
void lovrMeshSetVertexMap(Mesh* mesh, void* data, size_t count);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
bool lovrMeshIsRangeEnabled(Mesh* mesh);
void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled);
void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count);
void lovrMeshSetDrawRange(Mesh* mesh, int start, int count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
VertexPointer lovrMeshMap(Mesh* mesh, int start, size_t count, bool read, bool write);
void lovrMeshUnmap(Mesh* mesh);
