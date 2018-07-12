#include "graphics/opengl/opengl.h"
#include "graphics/gpu.h"
#include "math/math.h"
#include <limits.h>

GLenum lovrConvertMeshUsage(MeshUsage usage) {
  switch (usage) {
    case MESH_STATIC: return GL_STATIC_DRAW;
    case MESH_DYNAMIC: return GL_DYNAMIC_DRAW;
    case MESH_STREAM: return GL_STREAM_DRAW;
  }
}

GLenum lovrConvertMeshDrawMode(MeshDrawMode mode) {
  switch (mode) {
    case MESH_POINTS: return GL_POINTS;
    case MESH_LINES: return GL_LINES;
    case MESH_LINE_STRIP: return GL_LINE_STRIP;
    case MESH_LINE_LOOP: return GL_LINE_LOOP;
    case MESH_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case MESH_TRIANGLES: return GL_TRIANGLES;
    case MESH_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
  }
}

Mesh* lovrMeshCreate(uint32_t count, VertexFormat format, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  mesh->count = count;
  mesh->format = format;
  mesh->drawMode = drawMode;
  mesh->usage = lovrConvertMeshUsage(usage);

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  gpuBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, count * format.stride, NULL, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

  map_init(&mesh->attachments);
  for (int i = 0; i < format.count; i++) {
    map_set(&mesh->attachments, format.attributes[i].name, ((MeshAttachment) { mesh, i, 0, true }));
  }

  mesh->data.raw = calloc(count, format.stride);

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrRelease(mesh->material);
  free(mesh->data.raw);
  free(mesh->indices.raw);
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

void lovrMeshBind(Mesh* mesh, Shader* shader) {
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);

  MeshAttachment layout[MAX_ATTACHMENTS];
  memset(layout, 0, MAX_ATTACHMENTS * sizeof(MeshAttachment));

  gpuBindVertexArray(mesh->vao);
  lovrMeshUnmapVertices(mesh);
  lovrMeshUnmapIndices(mesh);
  if (mesh->indexCount > 0) {
    gpuBindIndexBuffer(mesh->ibo);
  }

  while ((key = map_next(&mesh->attachments, &iter)) != NULL) {
    int location = lovrShaderGetAttributeId(shader, key);

    if (location >= 0) {
      MeshAttachment* attachment = map_get(&mesh->attachments, key);
      layout[location] = *attachment;
      lovrMeshUnmapVertices(attachment->mesh);
      lovrMeshUnmapIndices(attachment->mesh);
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
      gpuBindVertexBuffer(current.mesh->vbo);
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

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) {
  uint32_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->count;
  lovrAssert(start + count <= limit, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
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

float* lovrMeshGetPose(Mesh* mesh) {
  return mesh->pose;
}

void lovrMeshSetPose(Mesh* mesh, float* pose) {
  mesh->pose = pose;
}

VertexPointer lovrMeshMapVertices(Mesh* mesh, uint32_t start, uint32_t count, bool read, bool write) {
  if (write) {
    mesh->dirtyStart = MIN(mesh->dirtyStart, start);
    mesh->dirtyEnd = MAX(mesh->dirtyEnd, start + count);
  }

  return (VertexPointer) { .bytes = mesh->data.bytes + start * mesh->format.stride };
}

void lovrMeshUnmapVertices(Mesh* mesh) {
  if (mesh->dirtyEnd == 0) {
    return;
  }

  size_t stride = mesh->format.stride;
  gpuBindVertexBuffer(mesh->vbo);
  if (mesh->usage == MESH_STREAM) {
    glBufferData(GL_ARRAY_BUFFER, mesh->count * stride, mesh->data.bytes, mesh->usage);
  } else {
    size_t offset = mesh->dirtyStart * stride;
    size_t count = (mesh->dirtyEnd - mesh->dirtyStart) * stride;
    glBufferSubData(GL_ARRAY_BUFFER, offset, count, mesh->data.bytes + offset);
  }

  mesh->dirtyStart = INT_MAX;
  mesh->dirtyEnd = 0;
}

IndexPointer lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* size) {
  *size = mesh->indexSize;
  *count = mesh->indexCount;

  if (mesh->indexCount == 0) {
    return (IndexPointer) { .raw = NULL };
  } else if (mesh->mappedIndices) {
    lovrMeshUnmapIndices(mesh);
  }

  return mesh->indices;
}

IndexPointer lovrMeshWriteIndices(Mesh* mesh, uint32_t count, size_t size) {
  if (mesh->mappedIndices) {
    lovrMeshUnmapIndices(mesh);
  }

  mesh->indexSize = size;
  mesh->indexCount = count;

  if (count == 0) {
    return (IndexPointer) { .raw = NULL };
  }

  gpuBindVertexArray(mesh->vao);
  gpuBindIndexBuffer(mesh->ibo);
  mesh->mappedIndices = true;

  if (mesh->indexCapacity < size * count) {
    mesh->indexCapacity = nextPo2(size * count);
    mesh->indices.raw = realloc(mesh->indices.raw, mesh->indexCapacity);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCapacity, NULL, mesh->usage);
  }

  return mesh->indices;
}

void lovrMeshUnmapIndices(Mesh* mesh) {
  if (!mesh->mappedIndices) {
    return;
  }

  mesh->mappedIndices = false;
  gpuBindIndexBuffer(mesh->ibo);
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->indexCount * mesh->indexSize, mesh->indices.raw);
}

void lovrMeshResize(Mesh* mesh, uint32_t count) {
  if (mesh->count < count) {
    mesh->count = nextPo2(count);
    gpuBindVertexBuffer(mesh->vbo);
    mesh->data.raw = realloc(mesh->data.raw, count * mesh->format.stride);
    glBufferData(GL_ARRAY_BUFFER, count * mesh->format.stride, mesh->data.raw, mesh->usage);
  }
}
