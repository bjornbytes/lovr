#include "graphics/mesh.h"
#include "graphics/graphics.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

static void lovrMeshBindAttributes(Mesh* mesh) {
  Shader* shader = lovrGraphicsGetActiveShader();
  if (shader == mesh->lastShader && !mesh->attributesDirty) {
    return;
  }

  lovrGraphicsBindVertexBuffer(mesh->vbo);

  for (int i = 0; i < mesh->format.count; i++) {
    Attribute attribute = mesh->format.attributes[i];
    int location = lovrShaderGetAttributeId(shader, attribute.name);

    if (location >= 0) {
      if (mesh->enabledAttributes & (1 << i)) {
        glEnableVertexAttribArray(location);

        GLenum glType;
        switch (attribute.type) {
          case ATTR_FLOAT: glType = GL_FLOAT; break;
          case ATTR_BYTE: glType = GL_UNSIGNED_BYTE; break;
          case ATTR_INT: glType = GL_UNSIGNED_INT; break;
        }

        if (attribute.type == ATTR_INT) {
          glVertexAttribIPointer(location, attribute.count, glType, mesh->format.stride, (void*) attribute.offset);
        } else {
          glVertexAttribPointer(location, attribute.count, glType, GL_TRUE, mesh->format.stride, (void*) attribute.offset);
        }
      } else {
        glDisableVertexAttribArray(location);
      }
    }
  }

  mesh->lastShader = shader;
  mesh->attributesDirty = false;
}

Mesh* lovrMeshCreate(size_t count, VertexFormat* format, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  if (format) {
    mesh->format = *format;
  } else {
    vertexFormatInit(&mesh->format);
    vertexFormatAppend(&mesh->format, "lovrPosition", ATTR_FLOAT, 3);
    vertexFormatAppend(&mesh->format, "lovrNormal", ATTR_FLOAT, 3);
    vertexFormatAppend(&mesh->format, "lovrTexCoord", ATTR_FLOAT, 2);
    vertexFormatAppend(&mesh->format, "lovrVertexColor", ATTR_BYTE, 4);
  }

  mesh->vertexCount = count;
  mesh->vertices.data = NULL;
  mesh->indices.data = NULL;
  mesh->indexCount = 0;
  mesh->indexSize = count > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
  mesh->enabledAttributes = ~0;
  mesh->attributesDirty = true;
  mesh->isMapped = false;
  mesh->mapStart = 0;
  mesh->mapCount = 0;
  mesh->isRangeEnabled = false;
  mesh->rangeStart = 0;
  mesh->rangeCount = count;
  mesh->drawMode = drawMode;
  mesh->usage = usage;
  mesh->vao = 0;
  mesh->vbo = 0;
  mesh->ibo = 0;
  mesh->material = NULL;
  mesh->lastShader = NULL;

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->vertexCount * mesh->format.stride, NULL, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

#ifdef EMSCRIPTEN
  mesh->vertices.data = malloc(mesh->vertexCount * mesh->format.stride);
#endif

  return mesh;
}

void lovrMeshDestroy(const Ref* ref) {
  Mesh* mesh = containerof(ref, Mesh);
  if (mesh->material) {
    lovrRelease(&mesh->material->ref);
  }
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteBuffers(1, &mesh->ibo);
  glDeleteVertexArrays(1, &mesh->vao);
  free(mesh->indices.data);
#ifdef EMSCRIPTEN
  free(mesh->vertices.data);
#endif
  free(mesh);
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
  return &mesh->format;
}

MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->drawMode;
}

void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode) {
  mesh->drawMode = drawMode;
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->vertexCount;
}

IndexData lovrMeshGetVertexMap(Mesh* mesh, size_t* count) {
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
    mesh->indices.data = realloc(mesh->indices.data, count * mesh->indexSize);
  }

  mesh->indexCount = count;
  memcpy(mesh->indices.data, data, mesh->indexCount * mesh->indexSize);
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrGraphicsBindIndexBuffer(mesh->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * mesh->indexSize, mesh->indices.data, GL_STATIC_DRAW);
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  for (int i = 0; i < mesh->format.count; i++) {
    if (!strcmp(mesh->format.attributes[i].name, name)) {
      return mesh->enabledAttributes & (1 << i);
    }
  }

  return false;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  for (int i = 0; i < mesh->format.count; i++) {
    if (!strcmp(mesh->format.attributes[i].name, name)) {
      int mask = 1 << i;
      if (enable && !(mesh->enabledAttributes & mask)) {
        mesh->enabledAttributes |= mask;
        mesh->attributesDirty = true;
      } else if (!enable && (mesh->enabledAttributes & mask)) {
        mesh->enabledAttributes &= ~(1 << i);
        mesh->attributesDirty = true;
      }
    }
  }
}

bool lovrMeshIsRangeEnabled(Mesh* mesh) {
  return mesh->isRangeEnabled;
}

void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled) {
  mesh->isRangeEnabled = isEnabled;

  if (!isEnabled) {
    mesh->rangeStart = 0;
    mesh->rangeCount = mesh->vertexCount;
  }
}

void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, int start, int count) {
  size_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->vertexCount;
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
    if (mesh->material) {
      lovrRelease(&mesh->material->ref);
    }

    mesh->material = material;

    if (material) {
      lovrRetain(&material->ref);
    }
  }
}

VertexData lovrMeshMap(Mesh* mesh, int start, size_t count, bool read, bool write) {
#ifdef EMSCRIPTEN
  mesh->isMapped = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  return (VertexData) { .data = mesh->vertices.bytes + start * mesh->format.stride };
#else
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  mesh->isMapped = true;
  mesh->mapStart = start;
  mesh->mapCount = count;
  GLbitfield access = 0;
  access |= read ? GL_MAP_READ_BIT : 0;
  access |= write ? GL_MAP_WRITE_BIT : 0;
  access |= (write && start == 0 && count == mesh->vertexCount) ? GL_MAP_INVALIDATE_BUFFER_BIT : 0;
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  return (VertexData) { .data = glMapBufferRange(GL_ARRAY_BUFFER, start * mesh->format.stride, count * mesh->format.stride, access) };
#endif
}

void lovrMeshUnmap(Mesh* mesh) {
  if (!mesh->isMapped) {
    return;
  }

  mesh->isMapped = false;
  lovrGraphicsBindVertexBuffer(mesh->vbo);

#ifdef EMSCRIPTEN
  size_t start = mesh->mapStart * mesh->format.stride;
  size_t count = mesh->mapCount * mesh->format.stride;
  glBufferSubData(GL_ARRAY_BUFFER, start, count, mesh->vertices.bytes + start);
#else
  glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
}
