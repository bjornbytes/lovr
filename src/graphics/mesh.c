#include "graphics/mesh.h"
#include "graphics/graphics.h"
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
      VertexFormat* format = &current.mesh->format;
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

Mesh* lovrMeshCreate(uint32_t count, VertexFormat format, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  mesh->count = count;
  mesh->format = format;
  mesh->drawMode = drawMode;
  mesh->usage = usage;

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, count * format.stride, NULL, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

  map_init(&mesh->attachments);
  for (int i = 0; i < format.count; i++) {
    map_set(&mesh->attachments, format.attributes[i].name, ((MeshAttachment) { mesh, i, 0, true }));
  }

#ifdef EMSCRIPTEN
  mesh->data.raw = calloc(count, format.stride);
#endif

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrRelease(mesh->material);
#ifdef EMSCRIPTEN
  free(mesh->data.raw);
  free(mesh->indices.raw);
#endif
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
  lovrMeshUnmapVertices(mesh);
  lovrMeshUnmapIndices(mesh);

  if (transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(transform);
  }

  lovrGraphicsPrepare(mesh->material, pose);
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrMeshBindAttributes(mesh);
  size_t start = mesh->rangeStart;
  size_t count = mesh->rangeCount ? mesh->rangeCount : mesh->count;
  if (mesh->indexCount > 0) {
    size_t offset = start * mesh->indexSize;
    count = mesh->rangeCount ? mesh->rangeCount : mesh->indexCount;
    lovrGraphicsDrawElements(mesh->drawMode, count, mesh->indexSize, offset, instances);
  } else {
    lovrGraphicsDrawArrays(mesh->drawMode, start, count, instances);
  }

  if (transform) {
    lovrGraphicsPop();
  }
}

VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh) {
  return &mesh->format;
}

MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->drawMode;
}

void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode) {
  mesh->drawMode = drawMode;
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->count;
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

void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, int start, int count) {
  size_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->count;
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

VertexPointer lovrMeshMapVertices(Mesh* mesh, uint32_t start, uint32_t count, bool read, bool write) {
#ifdef EMSCRIPTEN
  mesh->mappedVertices = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  return (VertexPointer) { .bytes = mesh->data.bytes + start * mesh->format.stride };
#else
  if (mesh->mappedVertices) {
    lovrMeshUnmapVertices(mesh);
  }

  mesh->mappedVertices = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  size_t stride = mesh->format.stride;
  GLbitfield access = (read ? GL_MAP_READ_BIT : 0) | (write ? GL_MAP_WRITE_BIT : 0);
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  return (VertexPointer) { .raw = glMapBufferRange(GL_ARRAY_BUFFER, start * stride, count * stride, access) };
#endif
}

void lovrMeshUnmapVertices(Mesh* mesh) {
  if (!mesh->mappedVertices) {
    return;
  }

  mesh->mappedVertices = false;
  lovrGraphicsBindVertexBuffer(mesh->vbo);

#ifdef EMSCRIPTEN
  size_t stride = mesh->format.stride;
  size_t start = mesh->mapStart * stride;
  size_t count = mesh->mapCount * stride;
  glBufferSubData(GL_ARRAY_BUFFER, start, count, mesh->data.bytes + start);
#else
  glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
}

IndexPointer lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* size) {
  if (mesh->indexCount == 0) {
    return (IndexPointer) { .raw = NULL };
  }

  *size = mesh->indexSize;
  *count = mesh->indexCount;

#ifdef EMSCRIPTEN
  return mesh->indices;
#endif

  lovrGraphicsBindIndexBuffer(mesh->ibo);
  mesh->mappedIndices = true;
  return (IndexPointer) { .raw = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->indexCount * mesh->indexSize, GL_MAP_READ_BIT) };
}

IndexPointer lovrMeshWriteIndices(Mesh* mesh, uint32_t count, size_t size) {
  mesh->indexSize = size;
  mesh->indexCount = count;

  if (count == 0) {
    return (IndexPointer) { .raw = NULL };
  }

  lovrGraphicsBindVertexArray(mesh->vao);
  lovrGraphicsBindIndexBuffer(mesh->ibo);
  mesh->mappedIndices = true;

  if (mesh->indexCapacity < size * count) {
    mesh->indexCapacity = nextPo2(size * count);
#ifdef EMSCRIPTEN
    mesh->indices = realloc(mesh->indices.raw, mesh->indexCapacity);
#else
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCapacity, NULL, mesh->usage);
#endif
  }

#ifdef EMSCRIPTEN
  return mesh->indices;
#else
  return (IndexPointer) { .raw = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size * count, GL_MAP_WRITE_BIT) };
#endif
}

void lovrMeshUnmapIndices(Mesh* mesh) {
  if (!mesh->mappedIndices) {
    return;
  }

  mesh->mappedIndices = false;
  lovrGraphicsBindIndexBuffer(mesh->ibo);
#ifdef EMSCRIPTEN
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->indexCount * mesh->indexSize, mesh->indices.raw);
#else
  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif
}

void lovrMeshResize(Mesh* mesh, uint32_t count) {
  mesh->count = count;
  mesh->mappedVertices = false;
  lovrGraphicsBindVertexBuffer(mesh->vbo);
#ifdef EMSCRIPTEN
  mesh->data.raw = realloc(mesh->data.raw, count * mesh->format.stride);
  glBufferData(GL_ARRAY_BUFFER, count * mesh->format.stride, mesh->data.raw, mesh->usage);
#else
  glBufferData(GL_ARRAY_BUFFER, count * mesh->format.stride, NULL, mesh->usage);
#endif
}
