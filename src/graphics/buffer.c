#include "graphics/buffer.h"
#include "graphics/graphics.h"
#include "graphics/shader.h"
#include <stdlib.h>

Buffer* lovrBufferCreate(int size, BufferFormat* format, BufferDrawMode drawMode, BufferUsage usage) {
  Buffer* buffer = lovrAlloc(sizeof(Buffer), lovrBufferDestroy);
  if (!buffer) return NULL;

  vec_init(&buffer->map);
  vec_init(&buffer->format);

  if (format) {
    vec_extend(&buffer->format, format);
  } else {
    BufferAttribute position = { .name = "lovrPosition", .type = BUFFER_FLOAT, .count = 3 };
    BufferAttribute normal = { .name = "lovrNormal", .type = BUFFER_FLOAT, .count = 3 };
    BufferAttribute texCoord = { .name = "lovrTexCoord", .type = BUFFER_FLOAT, .count = 2 };
    vec_push(&buffer->format, position);
    vec_push(&buffer->format, normal);
    vec_push(&buffer->format, texCoord);
  }

  int stride = 0;
  int i;
  BufferAttribute attribute;
  vec_foreach(&buffer->format, attribute, i) {
    stride += attribute.count * sizeof(attribute.type);
  }

  if (stride == 0) {
    return NULL;
  }

  buffer->size = size;
  buffer->stride = stride;
  buffer->data = malloc(buffer->size * buffer->stride);
  buffer->scratchVertex = malloc(buffer->stride);
  buffer->drawMode = drawMode;
  buffer->usage = usage;
  buffer->vbo = 0;
  buffer->ibo = 0;
  buffer->isRangeEnabled = 0;
  buffer->rangeStart = 0;
  buffer->rangeCount = buffer->size;
  buffer->texture = NULL;

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);
  glGenBuffers(1, &buffer->ibo);

  return buffer;
}

void lovrBufferDestroy(const Ref* ref) {
  Buffer* buffer = containerof(ref, Buffer);
  if (buffer->texture) {
    lovrRelease(&buffer->texture->ref);
  }
  glDeleteBuffers(1, &buffer->vbo);
  vec_deinit(&buffer->map);
  vec_deinit(&buffer->format);
  free(buffer->scratchVertex);
  free(buffer->data);
  free(buffer);
}

void lovrBufferDraw(Buffer* buffer) {
  int usingIbo = buffer->map.length > 0;

  lovrGraphicsPrepare();
  lovrGraphicsBindTexture(buffer->texture);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);

  // Figure out how many vertex attributes there are
  int vertexAttributeCount;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &vertexAttributeCount);

  // Disable all vertex attributes
  for (int i = 0; i < vertexAttributeCount; i++) {
    glDisableVertexAttribArray(i);
  }

  // Enable the vertex attributes in use and bind the correct data FIXME
  Shader* shader = lovrGraphicsGetShader();
  if (shader) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
    size_t offset = 0;
    int i;
    BufferAttribute attribute;
    vec_foreach(&buffer->format, attribute, i) {
      int location = lovrShaderGetAttributeId(shader, attribute.name);
      if (location >= 0) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, attribute.count, attribute.type, GL_FALSE, buffer->stride, (void*) offset);
      }
      offset += sizeof(attribute.type) * attribute.count;
    }
  }

  // Determine range of vertices to be rendered and whether we're using an IBO or not
  int start, count;
  if (buffer->isRangeEnabled) {
    start = buffer->rangeStart;
    count = buffer->rangeCount;
  } else {
    start = 0;
    count = usingIbo ? buffer->map.length : buffer->size;
  }

  // Render!  Use the IBO if a draw range is set
  if (usingIbo) {
    uintptr_t startAddress = (uintptr_t) start;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
    glDrawElements(buffer->drawMode, count, GL_UNSIGNED_INT, (GLvoid*) startAddress);
  } else {
    glDrawArrays(buffer->drawMode, start, count);
  }
}

BufferFormat lovrBufferGetVertexFormat(Buffer* buffer) {
  return buffer->format;
}

BufferDrawMode lovrBufferGetDrawMode(Buffer* buffer) {
  return buffer->drawMode;
}

int lovrBufferSetDrawMode(Buffer* buffer, BufferDrawMode drawMode) {
  buffer->drawMode = drawMode;
  return 0;
}

int lovrBufferGetVertexCount(Buffer* buffer) {
  return buffer->size;
}

int lovrBufferGetVertexSize(Buffer* buffer) {
  return buffer->stride;
}

void* lovrBufferGetScratchVertex(Buffer* buffer) {
  return buffer->scratchVertex;
}

void lovrBufferGetVertex(Buffer* buffer, int index, void* dest) {
  if (index >= buffer->size) {
    return;
  }

  memcpy(dest, (char*) buffer->data + index * buffer->stride, buffer->stride);
}

void lovrBufferSetVertex(Buffer* buffer, int index, void* vertex) {
  memcpy((char*) buffer->data + index * buffer->stride, vertex, buffer->stride);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);
}

void lovrBufferSetVertices(Buffer* buffer, void* vertices, int size) {
  if (size > buffer->size * buffer->stride) {
    error("Buffer is not big enough");
  }

  memcpy(buffer->data, vertices, size);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);
}

unsigned int* lovrBufferGetVertexMap(Buffer* buffer, int* count) {
  *count = buffer->map.length;
  return buffer->map.data;
}

void lovrBufferSetVertexMap(Buffer* buffer, unsigned int* map, int count) {
  if (count == 0 || !map) {
    vec_clear(&buffer->map);
  } else {
    vec_clear(&buffer->map);
    vec_pusharr(&buffer->map, map, count);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), buffer->map.data, GL_STATIC_DRAW);
  }
}

char lovrBufferIsRangeEnabled(Buffer* buffer) {
  return buffer->isRangeEnabled;
}

void lovrBufferSetRangeEnabled(Buffer* buffer, char isEnabled) {
  buffer->isRangeEnabled = isEnabled;
}

void lovrBufferGetDrawRange(Buffer* buffer, int* start, int* count) {
  *start = buffer->rangeStart;
  *count = buffer->rangeCount;
}

int lovrBufferSetDrawRange(Buffer* buffer, int start, int count) {
  if (start < 0 || count < 0 || start + count > buffer->size) {
    return 1;
  }

  buffer->rangeStart = start;
  buffer->rangeCount = count;

  return 0;
}

Texture* lovrBufferGetTexture(Buffer* buffer) {
  return buffer->texture;
}

void lovrBufferSetTexture(Buffer* buffer, Texture* texture) {
  if (buffer->texture) {
    lovrRelease(&buffer->texture->ref);
  }

  buffer->texture = texture;

  if (buffer->texture) {
    lovrRetain(&buffer->texture->ref);
  }
}
