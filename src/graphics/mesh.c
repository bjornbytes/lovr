#include "graphics/mesh.h"
#include "graphics/graphics.h"

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

int lovrMeshGetAttributeCount(Mesh* mesh) {
  return mesh->attributeCount;
}

void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute) {
  lovrAssert(!map_get(&mesh->attributeMap, name), "Mesh already has an attribute named '%s'", name);
  lovrAssert(mesh->attributeCount < MAX_ATTRIBUTES, "Mesh already has the max number of attributes (%d)", MAX_ATTRIBUTES);
  lovrGraphicsFlushMesh(mesh);
  int index = mesh->attributeCount++;
  mesh->attributes[index] = *attribute;
  mesh->attributeNames[index] = name;
  map_set(&mesh->attributeMap, name, index);
  lovrRetain(attribute->buffer);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  int* index = map_get(&mesh->attributeMap, name);
  lovrAssert(index, "No attached attribute named '%s' was found", name);
  MeshAttribute* attribute = &mesh->attributes[*index];
  lovrGraphicsFlushMesh(mesh);
  lovrRelease(attribute->buffer);
  map_remove(&mesh->attributeMap, name);
  mesh->attributeNames[*index] = NULL;
  memmove(mesh->attributeNames + *index, mesh->attributeNames + *index + 1, (mesh->attributeCount - *index - 1) * sizeof(char*));
  memmove(mesh->attributes + *index, mesh->attributes + *index + 1, (mesh->attributeCount - *index - 1) * sizeof(MeshAttribute));
  mesh->attributeCount--;
  for (int i = 0; i < MAX_ATTRIBUTES; i++) {
    if (mesh->locations[i] > *index) {
      mesh->locations[i]--;
    } else if (mesh->locations[i] == *index) {
      mesh->locations[i] = 0xff;
    }
  }
}

const MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, const char* name) {
  int* index = map_get(&mesh->attributeMap, name);
  return index ? &mesh->attributes[*index] : NULL;
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  int* index = map_get(&mesh->attributeMap, name);
  lovrAssert(index, "Mesh does not have an attribute named '%s'", name);
  return !mesh->attributes[*index].disabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  bool disable = !enable;
  int* index = map_get(&mesh->attributeMap, name);
  lovrAssert(index, "Mesh does not have an attribute named '%s'", name);
  if (mesh->attributes[*index].disabled != disable) {
    lovrGraphicsFlushMesh(mesh);
    mesh->attributes[*index].disabled = disable;
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
