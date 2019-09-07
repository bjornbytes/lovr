#include "graphics/mesh.h"
#include "graphics/buffer.h"
#include "graphics/graphics.h"
#include "graphics/material.h"
#include "core/hash.h"
#include "core/ref.h"
#include <stdlib.h>

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

void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute) {
  uint64_t hash = hash64(name, strlen(name));
  lovrAssert(map_get(&mesh->attributeMap, hash) == MAP_NIL, "Mesh already has an attribute named '%s'", name);
  lovrAssert(mesh->attributeCount < MAX_ATTRIBUTES, "Mesh already has the max number of attributes (%d)", MAX_ATTRIBUTES);
  lovrAssert(strlen(name) < MAX_ATTRIBUTE_NAME_LENGTH, "Mesh attribute name '%s' is too long (max is %d)", name, MAX_ATTRIBUTE_NAME_LENGTH);
  lovrGraphicsFlushMesh(mesh);
  uint64_t index = mesh->attributeCount++;
  mesh->attributes[index] = *attribute;
  strcpy(mesh->attributeNames[index], name);
  map_set(&mesh->attributeMap, hash, index);
  lovrRetain(attribute->buffer);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "No attached attribute named '%s' was found", name);
  MeshAttribute* attribute = &mesh->attributes[index];
  lovrGraphicsFlushMesh(mesh);
  lovrRelease(Buffer, attribute->buffer);
  map_remove(&mesh->attributeMap, hash);
  mesh->attributeNames[index][0] = '\0';
  memmove(mesh->attributeNames + index, mesh->attributeNames + index + 1, (mesh->attributeCount - index - 1) * MAX_ATTRIBUTE_NAME_LENGTH * sizeof(char));
  memmove(mesh->attributes + index, mesh->attributes + index + 1, (mesh->attributeCount - index - 1) * sizeof(MeshAttribute));
  mesh->attributeCount--;
  for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++) {
    if (mesh->locations[i] > index) {
      mesh->locations[i]--;
    } else if (mesh->locations[i] == index) {
      mesh->locations[i] = 0xff;
    }
  }
}

const MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  return index == MAP_NIL ? NULL : &mesh->attributes[index];
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "Mesh does not have an attribute named '%s'", name);
  return !mesh->attributes[index].disabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  bool disable = !enable;
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "Mesh does not have an attribute named '%s'", name);
  if (mesh->attributes[index].disabled != disable) {
    lovrGraphicsFlushMesh(mesh);
    mesh->attributes[index].disabled = disable;
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
  lovrRelease(Material, mesh->material);
  mesh->material = material;
}
