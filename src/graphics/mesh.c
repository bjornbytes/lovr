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

  size_t offset = 0;
  int i;
  MeshAttribute attribute;

  vec_foreach(&mesh->format, attribute, i) {
    int location = lovrShaderGetAttributeId(shader, attribute.name);

    if (location >= 0) {
      if (mesh->enabledAttributes & (1 << i)) {
        glEnableVertexAttribArray(location);

        if (attribute.type == MESH_INT) {
          glVertexAttribIPointer(location, attribute.count, attribute.type, mesh->stride, (void*) offset);
        } else {
          glVertexAttribPointer(location, attribute.count, attribute.type, GL_FALSE, mesh->stride, (void*) offset);
        }
      } else {
        glDisableVertexAttribArray(location);
      }
    }

    offset += sizeof(attribute.type) * attribute.count;
  }

  mesh->lastShader = shader;
  mesh->attributesDirty = 0;
}

Mesh* lovrMeshCreate(size_t count, MeshFormat* format, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  vec_init(&mesh->format);

  if (format) {
    vec_extend(&mesh->format, format);
  } else {
    MeshAttribute position = { .name = "lovrPosition", .type = MESH_FLOAT, .count = 3 };
    MeshAttribute normal = { .name = "lovrNormal", .type = MESH_FLOAT, .count = 3 };
    MeshAttribute texCoord = { .name = "lovrTexCoord", .type = MESH_FLOAT, .count = 2 };
    MeshAttribute vertexColor = { .name = "lovrVertexColor", .type = MESH_BYTE, .count = 4 };
    vec_push(&mesh->format, position);
    vec_push(&mesh->format, normal);
    vec_push(&mesh->format, texCoord);
    vec_push(&mesh->format, vertexColor);
  }

  int stride = 0;
  int i;
  MeshAttribute attribute;
  vec_foreach(&mesh->format, attribute, i) {
    stride += attribute.count * sizeof(attribute.type);
  }

  if (stride == 0) {
    return NULL;
  }

  mesh->data = NULL;
  mesh->count = count;
  mesh->stride = stride;
  mesh->enabledAttributes = ~0;
  mesh->attributesDirty = 1;
  mesh->isMapped = 0;
  mesh->drawMode = drawMode;
  mesh->usage = usage;
  mesh->vao = 0;
  mesh->vbo = 0;
  mesh->ibo = 0;
  mesh->indices = NULL;
  mesh->indexCount = 0;
  mesh->indexSize = count > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
  mesh->isRangeEnabled = 0;
  mesh->rangeStart = 0;
  mesh->rangeCount = mesh->count;
  mesh->lastShader = NULL;

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->count * mesh->stride, NULL, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

#ifdef EMSCRIPTEN
  mesh->data = malloc(mesh->count * mesh->stride);
#endif

  return mesh;
}

void lovrMeshDestroy(const Ref* ref) {
  Mesh* mesh = containerof(ref, Mesh);
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteBuffers(1, &mesh->ibo);
  glDeleteVertexArrays(1, &mesh->vao);
  vec_deinit(&mesh->format);
  free(mesh->indices);
#ifdef EMSCRIPTEN
  free(mesh->data);
#endif
  free(mesh);
}

void lovrMeshDraw(Mesh* mesh, mat4 transform) {
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  if (transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  }

  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsPrepare();
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrMeshBindAttributes(mesh);
  size_t start = mesh->rangeStart;
  size_t count = mesh->rangeCount;
  if (mesh->indexCount > 0) {
    count = mesh->isRangeEnabled ? mesh->rangeCount : mesh->indexCount;
    GLenum indexType = mesh->indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(mesh->drawMode, count, indexType, (GLvoid*) (start * mesh->indexSize));
  } else {
    glDrawArrays(mesh->drawMode, start, count);
  }

  if (transform) {
    lovrGraphicsPop();
  }
}

MeshFormat lovrMeshGetVertexFormat(Mesh* mesh) {
  return mesh->format;
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

int lovrMeshGetVertexSize(Mesh* mesh) {
  return mesh->stride;
}

void* lovrMeshGetVertexMap(Mesh* mesh, size_t* count) {
  *count = mesh->indexCount;
  return mesh->indices;
}

void lovrMeshSetVertexMap(Mesh* mesh, void* data, size_t count) {
  if (!data || count == 0) {
    mesh->indexCount = 0;
    return;
  }

  mesh->indices = data;
  mesh->indexCount = count;
  lovrGraphicsBindVertexArray(mesh->vao);
  lovrGraphicsBindIndexBuffer(mesh->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * mesh->indexSize, mesh->indices, GL_STATIC_DRAW);
}

int lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  int i;
  MeshAttribute attribute;

  vec_foreach(&mesh->format, attribute, i) {
    if (!strcmp(attribute.name, name)) {
      return mesh->enabledAttributes & (1 << i);
    }
  }

  return 0;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, int enable) {
  int i;
  MeshAttribute attribute;

  vec_foreach(&mesh->format, attribute, i) {
    if (!strcmp(attribute.name, name)) {
      int mask = 1 << i;
      if (enable && !(mesh->enabledAttributes & mask)) {
        mesh->enabledAttributes |= mask;
        mesh->attributesDirty = 1;
      } else if (!enable && (mesh->enabledAttributes & mask)) {
        mesh->enabledAttributes &= ~(1 << i);
        mesh->attributesDirty = 1;
      }
    }
  }
}

int lovrMeshIsRangeEnabled(Mesh* mesh) {
  return mesh->isRangeEnabled;
}

void lovrMeshSetRangeEnabled(Mesh* mesh, char isEnabled) {
  mesh->isRangeEnabled = isEnabled;

  if (!isEnabled) {
    mesh->rangeStart = 0;
    mesh->rangeCount = mesh->count;
  }
}

void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, int start, int count) {
  size_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->count;
  int isValidRange = start >= 0 && count >= 0 && (size_t) start + count <= limit;
  lovrAssert(isValidRange, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  mesh->rangeStart = start;
  mesh->rangeCount = count;
}

void* lovrMeshMap(Mesh* mesh, int start, size_t count, int read, int write) {
#ifdef EMSCRIPTEN
  mesh->isMapped = 1;
  mesh->mapStart = start;
  mesh->mapCount = count;
  return (char*) mesh->data + start * mesh->stride;
#else
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  mesh->isMapped = 1;
  mesh->mapStart = start;
  mesh->mapCount = count;
  GLbitfield access = 0;
  access |= read ? GL_MAP_READ_BIT : 0;
  access |= write ? GL_MAP_WRITE_BIT : 0;
  access |= (write && start == 0 && count == mesh->count) ? GL_MAP_INVALIDATE_BUFFER_BIT : 0;
  lovrGraphicsBindVertexBuffer(mesh->vbo);
  return glMapBufferRange(GL_ARRAY_BUFFER, start * mesh->stride, count * mesh->stride, access);
#endif
}

void lovrMeshUnmap(Mesh* mesh) {
  if (!mesh->isMapped) {
    return;
  }

  mesh->isMapped = 0;
  lovrGraphicsBindVertexBuffer(mesh->vbo);

#ifdef EMSCRIPTEN
  int start = mesh->mapStart * mesh->stride;
  size_t count = mesh->mapCount * mesh->stride;
  glBufferSubData(GL_ARRAY_BUFFER, start, count, (char*) mesh->data + start);
#else
  glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
}
