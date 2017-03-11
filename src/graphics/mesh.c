#include "graphics/mesh.h"
#include "graphics/graphics.h"
#include <stdlib.h>

static void lovrMeshBindAttributes(Mesh* mesh) {
  Shader* shader = lovrGraphicsGetShader();
  if (!shader || (shader == mesh->lastShader && !mesh->attributesDirty)) {
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

      if (attribute.type == MESH_INT) {
        glVertexAttribIPointer(location, attribute.count, attribute.type, mesh->stride, (void*) offset);
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

Mesh* lovrMeshCreate(int size, MeshFormat* format, MeshDrawMode drawMode, MeshUsage usage) {
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

  mesh->size = size;
  mesh->stride = stride;
  mesh->data = malloc(mesh->size * mesh->stride);
  mesh->scratchVertex = malloc(mesh->stride);
  mesh->enabledAttributes = ~0;
  mesh->attributesDirty = 1;
  mesh->drawMode = drawMode;
  mesh->usage = usage;
  mesh->vao = 0;
  mesh->vbo = 0;
  mesh->ibo = 0;
  mesh->isRangeEnabled = 0;
  mesh->rangeStart = 0;
  mesh->rangeCount = mesh->size;
  mesh->texture = NULL;
  mesh->lastShader = NULL;

  glGenBuffers(1, &mesh->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->size * mesh->stride, mesh->data, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);
  glGenBuffers(1, &mesh->ibo);

  return mesh;
}

void lovrMeshDestroy(const Ref* ref) {
  Mesh* mesh = containerof(ref, Mesh);
  if (mesh->texture) {
    lovrRelease(&mesh->texture->ref);
  }
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteVertexArrays(1, &mesh->vao);
  vec_deinit(&mesh->map);
  vec_deinit(&mesh->format);
  free(mesh->scratchVertex);
  free(mesh->data);
  free(mesh);
}

void lovrMeshDraw(Mesh* mesh, mat4 transform) {
  int usingIbo = mesh->map.length > 0;

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsBindTexture(mesh->texture);
  lovrGraphicsPrepare();

  glBindVertexArray(mesh->vao);
  lovrMeshBindAttributes(mesh);

  // Determine range of vertices to be rendered and whether we're using an IBO or not
  int start, count;
  if (mesh->isRangeEnabled) {
    start = mesh->rangeStart;
    count = mesh->rangeCount;
  } else {
    start = 0;
    count = usingIbo ? mesh->map.length : mesh->size;
  }

  // Render!  Use the IBO if a draw range is set
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
  return mesh->size;
}

int lovrMeshGetVertexSize(Mesh* mesh) {
  return mesh->stride;
}

void* lovrMeshGetScratchVertex(Mesh* mesh) {
  return mesh->scratchVertex;
}

void lovrMeshGetVertex(Mesh* mesh, int index, void* dest) {
  if (index >= mesh->size) {
    return;
  }

  memcpy(dest, (char*) mesh->data + index * mesh->stride, mesh->stride);
}

void lovrMeshSetVertex(Mesh* mesh, int index, void* vertex) {
  memcpy((char*) mesh->data + index * mesh->stride, vertex, mesh->stride);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->size * mesh->stride, mesh->data, mesh->usage);
}

void lovrMeshSetVertices(Mesh* mesh, void* vertices, int size) {
  if (size > mesh->size * mesh->stride) {
    error("Mesh is not big enough");
  }

  memcpy(mesh->data, vertices, size);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->size * mesh->stride, mesh->data, mesh->usage);
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
  if (start < 0 || count < 0 || start + count > mesh->size) {
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
