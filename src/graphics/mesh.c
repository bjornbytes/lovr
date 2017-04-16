#include "graphics/mesh.h"
#include "graphics/graphics.h"
#include <stdlib.h>

static void lovrMeshBindAttributes(Mesh* mesh) {
  Shader* shader = lovrGraphicsGetShader();
  if (!shader || (shader == mesh->lastShader && !mesh->attributesDirty && GLAD_GL_VERSION_3_0)) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);

  size_t offset = 0;
  int i;
  MeshAttribute attribute;

  vec_foreach(&mesh->format, attribute, i) {
    int location = lovrShaderGetAttributeId(shader, attribute.name);

    if (location >= 0 && (mesh->enabledAttributes & (1 << i))) {
      glEnableVertexAttribArray(location);

      if (attribute.type == MESH_INT ) {
        if (GLAD_GL_ES_VERSION_2_0) {
          error("Integer attributes are not supported on this platform.");
        } else {
          glVertexAttribIPointer(location, attribute.count, attribute.type, mesh->stride, (void*) offset);
        }
      } else {
        glVertexAttribPointer(location, attribute.count, attribute.type, GL_FALSE, mesh->stride, (void*) offset);
      }
    } else {
      glDisableVertexAttribArray(location);
    }

    offset += sizeof(attribute.type) * attribute.count;
  }

  mesh->lastShader = shader;
  mesh->attributesDirty = 0;
}

Mesh* lovrMeshCreate(int count, MeshFormat* format, MeshDrawMode drawMode, MeshUsage usage) {
  Mesh* mesh = lovrAlloc(sizeof(Mesh), lovrMeshDestroy);
  if (!mesh) return NULL;

  vec_init(&mesh->map);
  vec_init(&mesh->format);

  if (format) {
    vec_extend(&mesh->format, format);
  } else {
    MeshAttribute position = { .name = "lovrPosition", .type = MESH_FLOAT, .count = 3 };
    MeshAttribute normal = { .name = "lovrNormal", .type = MESH_FLOAT, .count = 3 };
    MeshAttribute texCoord = { .name = "lovrTexCoord", .type = MESH_FLOAT, .count = 2 };
    vec_push(&mesh->format, position);
    vec_push(&mesh->format, normal);
    vec_push(&mesh->format, texCoord);
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
  mesh->drawMode = drawMode;
  mesh->usage = usage;
  mesh->vao = 0;
  mesh->vbo = 0;
  mesh->ibo = 0;
  mesh->isRangeEnabled = 0;
  mesh->rangeStart = 0;
  mesh->rangeCount = mesh->count;
  mesh->texture = NULL;
  mesh->lastShader = NULL;

  glGenBuffers(1, &mesh->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->count * mesh->stride, NULL, mesh->usage);
  if (GLAD_GL_VERSION_3_0) {
    glGenVertexArrays(1, &mesh->vao);
  }
  glGenBuffers(1, &mesh->ibo);

  if (!GLAD_GL_VERSION_3_0 && !GL_ARB_map_buffer_range) {
    mesh->data = malloc(mesh->count * mesh->stride);
  }

  return mesh;
}

void lovrMeshDestroy(const Ref* ref) {
  Mesh* mesh = containerof(ref, Mesh);
  if (mesh->texture) {
    lovrRelease(&mesh->texture->ref);
  }
  glDeleteBuffers(1, &mesh->vbo);
  if (GLAD_GL_VERSION_3_0) {
    glDeleteVertexArrays(1, &mesh->vao);
  }
  vec_deinit(&mesh->map);
  vec_deinit(&mesh->format);
  free(mesh->data);
  free(mesh);
}

void lovrMeshDraw(Mesh* mesh, mat4 transform) {
  if (mesh->isMapped) {
    lovrMeshUnmap(mesh);
  }

  int usingIbo = mesh->map.length > 0;

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsBindTexture(mesh->texture);
  lovrGraphicsPrepare();

  if (GLAD_GL_VERSION_3_0) {
    glBindVertexArray(mesh->vao);
  }

  lovrMeshBindAttributes(mesh);

  int start, count;
  if (mesh->isRangeEnabled) {
    start = mesh->rangeStart;
    count = mesh->rangeCount;
  } else {
    start = 0;
    count = usingIbo ? mesh->map.length : mesh->count;
  }

  if (usingIbo) {
    uintptr_t startAddress = (uintptr_t) start;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glDrawElements(mesh->drawMode, count, GL_UNSIGNED_INT, (GLvoid*) startAddress);
  } else {
    glDrawArrays(mesh->drawMode, start, count);
  }

  lovrGraphicsPop();
}

MeshFormat lovrMeshGetVertexFormat(Mesh* mesh) {
  return mesh->format;
}

MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->drawMode;
}

int lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode) {
  mesh->drawMode = drawMode;
  return 0;
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->count;
}

int lovrMeshGetVertexSize(Mesh* mesh) {
  return mesh->stride;
}

unsigned int* lovrMeshGetVertexMap(Mesh* mesh, int* count) {
  *count = mesh->map.length;
  return mesh->map.data;
}

void lovrMeshSetVertexMap(Mesh* mesh, unsigned int* map, int count) {
  if (count == 0 || !map) {
    vec_clear(&mesh->map);
  } else {
    vec_clear(&mesh->map);
    vec_pusharr(&mesh->map, map, count);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), mesh->map.data, GL_STATIC_DRAW);
  }
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
}

void lovrMeshGetDrawRange(Mesh* mesh, int* start, int* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

int lovrMeshSetDrawRange(Mesh* mesh, int start, int count) {
  if (start < 0 || count < 0 || start + count > mesh->count) {
    return 1;
  }

  mesh->rangeStart = start;
  mesh->rangeCount = count;

  return 0;
}

Texture* lovrMeshGetTexture(Mesh* mesh) {
  return mesh->texture;
}

void lovrMeshSetTexture(Mesh* mesh, Texture* texture) {
  if (mesh->texture) {
    lovrRelease(&mesh->texture->ref);
  }

  mesh->texture = texture;

  if (mesh->texture) {
    lovrRetain(&mesh->texture->ref);
  }
}

void* lovrMeshMap(Mesh* mesh, int start, int count) {
  if (!GLAD_GL_VERSION_3_0 && !GLAD_GL_ARB_map_buffer_range) {
    mesh->isMapped = 1;
    mesh->mapStart = start;
    mesh->mapCount = count;
    return (char*) mesh->data + start * mesh->stride;
  }

  // Unmap because the mapped ranges aren't necessarily the same.  Could be improved.
  if (mesh->isMapped && (mesh->mapStart != start || mesh->mapCount != count)) {
    lovrMeshUnmap(mesh);
  }

  mesh->isMapped = 1;
  mesh->mapStart = start;
  mesh->mapCount = count;
  GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  return glMapBufferRange(GL_ARRAY_BUFFER, start * mesh->stride, count * mesh->stride, access);
}

void lovrMeshUnmap(Mesh* mesh) {
  if (!GLAD_GL_VERSION_3_0 && !GLAD_GL_ARB_map_buffer_range) {
    mesh->isMapped = 0;
    int start = mesh->mapStart * mesh->stride;
    int count = mesh->mapCount * mesh->stride;
    char* data = (char*) mesh->data + count;
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, start, count, data);
    return;
  }

  if (mesh->isMapped) {
    mesh->isMapped = 0;
    glUnmapBuffer(GL_ARRAY_BUFFER);
  }
}
