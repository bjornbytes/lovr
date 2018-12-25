#include "graphics/mesh.h"

void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute) {
  lovrAssert(!map_get(&mesh->attributes, name), "Mesh already has an attribute named '%s'", name);
  lovrAssert(attribute->divisor >= 0, "Divisor can't be negative");
  map_set(&mesh->attributes, name, *attribute);
  lovrRetain(attribute->buffer);
  mesh->dirty = true;
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "No attached attribute '%s' was found", name);
  lovrAssert(attribute->buffer != mesh->vbo, "Attribute '%s' was not attached from another Mesh", name);
  lovrRelease(attribute->buffer);
  map_remove(&mesh->attributes, name);
  mesh->dirty = true;
}

MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name) {
  return map_get(&mesh->attributes, name);
}

bool lovrMeshIsDirty(Mesh* mesh) {
  return mesh->dirty;
}

VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh) {
  return &mesh->format;
}

bool lovrMeshIsReadable(Mesh* mesh) {
  return mesh->readable;
}

DrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->mode;
}

void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode) {
  if (mesh->mode != mode) {
    mesh->mode = mode;
    mesh->dirty = true;
  }
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->count;
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "Mesh does not have an attribute named '%s'", name);
  return attribute->enabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  MeshAttribute* attribute = map_get(&mesh->attributes, name);
  lovrAssert(attribute, "Mesh does not have an attribute named '%s'", name);
  mesh->dirty = attribute->enabled != enable;
  attribute->enabled = enable;
}

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) {
  uint32_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->count;
  lovrAssert(start + count <= limit, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  if (mesh->rangeStart != start || mesh->rangeCount != count) {
    mesh->rangeStart = start;
    mesh->rangeCount = count;
    mesh->dirty = true;
  }
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  if (mesh->material != material) {
    lovrRetain(material);
    lovrRelease(mesh->material);
    mesh->material = material;
    mesh->dirty = true;
  }
}

void* lovrMeshMapVertices(Mesh* mesh, size_t offset) {
  return lovrBufferMap(mesh->vbo, offset);
}

void lovrMeshFlushVertices(Mesh* mesh, size_t offset, size_t size) {
  lovrBufferFlush(mesh->vbo, offset, size);
}

void* lovrMeshMapIndices(Mesh* mesh, uint32_t count, size_t indexSize, size_t offset) {
  mesh->indexSize = indexSize;
  mesh->indexCount = count;
  mesh->dirty = true;

  if (count == 0) {
    return NULL;
  }

  if (mesh->indexCapacity < indexSize * count) {
    mesh->indexCapacity = nextPo2(indexSize * count);
    lovrRelease(mesh->ibo);
    mesh->ibo = lovrBufferCreate(mesh->indexCapacity, NULL, mesh->usage, mesh->readable);
  }

  return lovrBufferMap(mesh->ibo, offset);
}

void lovrMeshFlushIndices(Mesh* mesh) {
  if (mesh->indexCount > 0) {
    lovrBufferFlush(mesh->ibo, 0, mesh->indexCount * mesh->indexSize);
  }
}

void* lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* indexSize) {
  return *count = mesh->indexCount, *indexSize = mesh->indexSize, lovrBufferMap(mesh->ibo, 0);
}
