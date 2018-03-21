#include "graphics/mesh.h"
#include "graphics/graphics.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static void lovrMeshBindAttributes(Mesh* mesh) {
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);
  Shader* shader = lovrGraphicsGetActiveShader();

  MeshAttachment layout[MAX_ATTACHMENTS];
  memset(layout, 0, MAX_ATTACHMENTS * sizeof(MeshAttachment));

  while ((key = map_next(&mesh->attachments, &iter)) != NULL) {
    int location = lovrShaderGetAttributeId(shader, key);

    if (location >= 0) {
      MeshAttachment* attachment = map_get(&mesh->attachments, key);
      layout[location] = *attachment;
    }
  }

  for (int i = 0; i < MAX_ATTACHMENTS; i++) {
    MeshAttachment previous = mesh->layout[i];
    MeshAttachment current = layout[i];

    if (!memcmp(&previous, &current, sizeof(MeshAttachment))) {
      continue;
    }

    if (previous.enabled != current.enabled) {
      if (current.enabled) {
        glEnableVertexAttribArray(i);
      } else {
        glDisableVertexAttribArray(i);
        mesh->layout[i] = current;
        continue;
      }
    }

    if (previous.divisor != current.divisor) {
      glVertexAttribDivisor(i, current.divisor);
    }

    if (previous.mesh != current.mesh || previous.attributeIndex != current.attributeIndex) {
      lovrGraphicsBindVertexBuffer(current.mesh->vbo);
      VertexFormat* format = &current.mesh->vertexData->format;
      Attribute attribute = format->attributes[current.attributeIndex];
      switch (attribute.type) {
        case ATTR_FLOAT:
          glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_TRUE, format->stride, (void*) attribute.offset);
          break;

        case ATTR_BYTE:
          glVertexAttribPointer(i, attribute.count, GL_UNSIGNED_BYTE, GL_TRUE, format->stride, (void*) attribute.offset);
          break;

        case ATTR_INT:
          glVertexAttribIPointer(i, attribute.count, GL_UNSIGNED_INT, format->stride, (void*) attribute.offset);
          break;
      }
    }

    mesh->layout[i] = current;
  }
}

Mesh* lovrMeshCreate(VertexData* vertexData, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  uint32_t count = vertexData->count;
  mesh->vertexData = vertexData;
  mesh->indexSize = count > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
  mesh->drawMode = drawMode;
  mesh->usage = usage;

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, count * mesh->vertexData->format.stride, vertexData->blob.data, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

  map_init(&mesh->attachments);
  for (int i = 0; i < vertexData->format.count; i++) {
    MeshAttachment attachment = { mesh, i, 0, true };
    map_set(&mesh->attachments, vertexData->format.attributes[i].name, attachment);
  }

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrRelease(mesh->material);
  lovrRelease(mesh->vertexData);
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteBuffers(1, &mesh->ibo);
  glDeleteVertexArrays(1, &mesh->vao);
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);
  while ((key = map_next(&mesh->attachments, &iter)) != NULL) {
    MeshAttachment* attachment = map_get(&mesh->attachments, key);
    if (attachment->mesh != mesh) {
      lovrRelease(attachment->mesh);
    }
  }
  map_deinit(&mesh->attachments);
  free(mesh->indices.raw);
  free(mesh);
}

void lovrMeshAttachAttribute(Mesh* mesh, Mesh* other, const char* name, int divisor) {
  MeshAttachment* otherAttachment = map_get(&other->attachments, name);
  lovrAssert(!mesh->isAttachment, "Attempted to attach to a mesh which is an attachment itself");
  lovrAssert(otherAttachment, "No attribute named '%s' exists", name);
  lovrAssert(!map_get(&mesh->attachments, name), "Mesh already has an attribute named '%s'", name);
  lovrAssert(divisor >= 0, "Divisor can't be negative");

  MeshAttachment attachment = { other, otherAttachment->attributeIndex, divisor, true };
  map_set(&mesh->attachments, name, attachment);
  other->isAttachment = true;
  lovrRetain(other);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "No attached attribute '%s' was found", name);
  lovrAssert(attachment->mesh != mesh, "Attribute '%s' was not attached from another Mesh", name);
  lovrRelease(attachment->mesh);
  map_remove(&mesh->attachments, name);
}

void lovrMeshDraw(Mesh* mesh, mat4 transform, float* pose, int instances) {
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  if (transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  }

  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsPrepare(mesh->material, pose);
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrMeshBindAttributes(mesh);
  size_t start = mesh->rangeStart;
  size_t count = mesh->rangeCount;
  if (mesh->indexCount > 0) {
    size_t offset = start * mesh->indexSize;
    count = mesh->isRangeEnabled ? mesh->rangeCount : mesh->indexCount;
    lovrGraphicsDrawElements(mesh->drawMode, count, mesh->indexSize, offset, instances);
  } else {
    lovrGraphicsDrawArrays(mesh->drawMode, start, count, instances);
  }

  if (transform) {
    lovrGraphicsPop();
  }
}

VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh) {
  return &mesh->vertexData->format;
}

MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->drawMode;
}

void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode) {
  mesh->drawMode = drawMode;
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->vertexData->count;
}

IndexPointer lovrMeshGetVertexMap(Mesh* mesh, size_t* count) {
  if (count) {
    *count = mesh->indexCount;
  }

  return mesh->indices;
}

void lovrMeshSetVertexMap(Mesh* mesh, void* data, size_t count) {
  if (!data || count == 0) {
    mesh->indexCount = 0;
    return;
  }

  if (mesh->indexCount < count) {
    mesh->indices.raw = realloc(mesh->indices.raw, count * mesh->indexSize);
  }

  mesh->indexCount = count;
  memcpy(mesh->indices.raw, data, mesh->indexCount * mesh->indexSize);
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrGraphicsBindIndexBuffer(mesh->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * mesh->indexSize, mesh->indices.raw, GL_STATIC_DRAW);
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "Mesh does not have an attribute named '%s'", name);
  return attachment->enabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "Mesh does not have an attribute named '%s'", name);
  attachment->enabled = enable;
}

bool lovrMeshIsRangeEnabled(Mesh* mesh) {
  return mesh->isRangeEnabled;
}

void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled) {
  mesh->isRangeEnabled = isEnabled;

  if (!isEnabled) {
    mesh->rangeStart = 0;
    mesh->rangeCount = mesh->vertexData->count;
  }
}

void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, int start, int count) {
  size_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->vertexData->count;
  bool isValidRange = start >= 0 && count >= 0 && (size_t) start + count <= limit;
  lovrAssert(isValidRange, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  mesh->rangeStart = start;
  mesh->rangeCount = count;
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  if (mesh->material != material) {
    lovrRetain(material);
    lovrRelease(mesh->material);
    mesh->material = material;
  }
}

VertexPointer lovrMeshMap(Mesh* mesh, int start, size_t count, bool read, bool write) {
#ifdef EMSCRIPTEN
  mesh->isMapped = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  VertexPointer pointer = { .raw = mesh->vertexData->blob.data };
  pointer.bytes += start * mesh->vertexData->format.stride;
  return pointer;
#else
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  mesh->isMapped = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  size_t stride = mesh->vertexData->format.stride;
  GLbitfield access = 0;
  access |= read ? GL_MAP_READ_BIT : 0;
  access |= write ? GL_MAP_WRITE_BIT : 0;
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  VertexPointer pointer;
  pointer.raw = glMapBufferRange(GL_ARRAY_BUFFER, start * stride, count * stride, access);
  return pointer;
#endif
}

void lovrMeshUnmap(Mesh* mesh) {
  if (!mesh->isMapped) {
    return;
  }

  mesh->isMapped = false;
  lovrGraphicsBindVertexBuffer(mesh->vbo);

#ifdef EMSCRIPTEN
  size_t stride = mesh->vertexData->format.stride;
  size_t start = mesh->mapStart * stride;
  size_t count = mesh->mapCount * stride;
  VertexPointer vertices = { .raw = mesh->vertexData->blob.data };
  glBufferSubData(GL_ARRAY_BUFFER, start, count, vertices.bytes + start);
#else
  glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
}
