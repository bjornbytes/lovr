#include "graphics/mesh.h"
#include "graphics/graphics.h"

VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh) {
  return &mesh->format;
}

Buffer* lovrMeshGetVertexBuffer(Mesh* mesh) {
  return mesh->vertexBuffer;
}

Buffer* lovrMeshGetIndexBuffer(Mesh* mesh) {
  return mesh->indexBuffer;
}

uint32_t lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->vertexCount;
}

uint32_t lovrMeshGetIndexCount(Mesh* mesh) {
  return mesh->indexCount;
}

size_t lovrMeshGetIndexSize(Mesh* mesh) {
  return mesh->indexSize;
}

void lovrMeshMarkVertices(Mesh* mesh, size_t start, size_t end) {
  mesh->flushStart = MIN(mesh->flushStart, start);
  mesh->flushEnd = MAX(mesh->flushEnd, end);
}

void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute) {
  lovrAssert(!map_get(&mesh->attributes, name), "Mesh already has an attribute named '%s'", name);
  lovrAssert(attribute->divisor >= 0, "Divisor can't be negative");
  lovrGraphicsFlushMesh(mesh);
  map_set(&mesh->attributes, name, *attribute);
  lovrRetain(attribute->buffer);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "No attached attribute '%s' was found", name);
  lovrAssert(attribute->buffer != mesh->vertexBuffer, "Attribute '%s' was not attached from another Mesh", name);
  lovrGraphicsFlushMesh(mesh);
  lovrRelease(attribute->buffer);
  map_remove(&mesh->attributes, name);
}

MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name) {
  return map_get(&mesh->attributes, name);
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "Mesh does not have an attribute named '%s'", name);
  return attribute->enabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "Mesh does not have an attribute named '%s'", name);
  if (attribute->enabled != enable) {
    lovrGraphicsFlushMesh(mesh);
    attribute->enabled = enable;
  }
}

DrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->mode;
}

void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode) {
  mesh->mode = mode;
}

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count) {
  *start = mesh->drawStart;
  *count = mesh->drawCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) {
  uint32_t limit = mesh->indexSize > 0 ? mesh->indexCount : mesh->vertexCount;
  lovrAssert(start + count <= limit, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  mesh->drawStart = start;
  mesh->drawCount = count;
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  lovrRetain(material);
  lovrRelease(mesh->material);
  mesh->material = material;
}
