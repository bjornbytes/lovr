#include "graphics/material.h"
#include "graphics/shader.h"
#include "data/vertexData.h"
#include <stdbool.h>

#pragma once

#define MAX_ATTACHMENTS 16

typedef enum {
  MESH_POINTS,
  MESH_LINES,
  MESH_LINE_STRIP,
  MESH_LINE_LOOP,
  MESH_TRIANGLE_STRIP,
  MESH_TRIANGLES,
  MESH_TRIANGLE_FAN
} MeshDrawMode;

typedef struct Mesh Mesh;

Mesh* lovrMeshCreate(uint32_t count, VertexFormat format, MeshDrawMode drawMode, BufferUsage usage);
void lovrMeshDestroy(void* ref);
void lovrMeshAttachAttribute(Mesh* mesh, Mesh* other, const char* name, int divisor);
void lovrMeshDetachAttribute(Mesh* mesh, const char* name);
void lovrMeshBind(Mesh* mesh, Shader* shader, int divisorMultiplier);
void lovrMeshDraw(Mesh* mesh, int instances);
VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh);
MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode);
int lovrMeshGetVertexCount(Mesh* mesh);
bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name);
void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enabled);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count);
void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);
float* lovrMeshGetPose(Mesh* mesh);
void lovrMeshSetPose(Mesh* mesh, float* pose);
VertexPointer lovrMeshMapVertices(Mesh* mesh, uint32_t start, uint32_t count, bool read, bool write);
void lovrMeshUnmapVertices(Mesh* mesh);
IndexPointer lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* size);
IndexPointer lovrMeshWriteIndices(Mesh* mesh, uint32_t count, size_t size);
void lovrMeshUnmapIndices(Mesh* mesh);
void lovrMeshResize(Mesh* mesh, uint32_t count);
