#include "buffer.h"
#include "graphics.h"
#include <stdlib.h>

Buffer* lovrBufferCreate(int size, BufferFormat* format, BufferDrawMode drawMode, BufferUsage usage) {
  Buffer* buffer = malloc(sizeof(Buffer));
  if (!buffer) return NULL;

  vec_init(&buffer->map);
  vec_init(&buffer->format);

  if (format) {
    vec_extend(&buffer->format, format);
  } else {
    BufferAttribute position = { .name = "position", .type = BUFFER_FLOAT, .size = 3 };
    BufferAttribute normal = { .name = "normal", .type = BUFFER_FLOAT, .size = 3 };
    BufferAttribute texCoord = { .name = "texCoord", .type = BUFFER_FLOAT, .size = 2 };
    vec_push(&buffer->format, position);
    vec_push(&buffer->format, normal);
    vec_push(&buffer->format, texCoord);
  }

  int stride = 0;
  int i;
  BufferAttribute attribute;
  vec_foreach(&buffer->format, attribute, i) {
    stride += attribute.size * sizeof(attribute.type);
  }

  buffer->size = size;
  buffer->stride = stride;
  buffer->data = malloc(buffer->size * buffer->stride);
  buffer->scratchVertex = malloc(buffer->stride);
  buffer->drawMode = drawMode;
  buffer->usage = usage;
  buffer->vao = 0;
  buffer->vbo = 0;
  buffer->ibo = 0;
  buffer->isRangeEnabled = 0;
  buffer->rangeStart = 0;
  buffer->rangeCount = buffer->size;
  buffer->texture = NULL;

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);
  glGenVertexArrays(1, &buffer->vao);

  return buffer;
}

void lovrBufferDestroy(Buffer* buffer) {
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteVertexArrays(1, &buffer->vao);
  vec_deinit(&buffer->map);
  vec_deinit(&buffer->format);
  free(buffer->scratchVertex);
  free(buffer->data);
  free(buffer);
}

void lovrBufferDraw(Buffer* buffer) {
  int usingIbo = buffer->map.length > 0;

  lovrGraphicsPrepare();
  glBindVertexArray(buffer->vao);
  for (int i = 0; i < buffer->format.length; i++) {
    glEnableVertexAttribArray(i);
  }

  if (buffer->texture) {
    glBindTexture(GL_TEXTURE_2D, buffer->texture->id);
  }

  int start, count;
  if (buffer->isRangeEnabled) {
    start = buffer->rangeStart;
    count = buffer->rangeCount;
  } else {
    start = 0;
    count = usingIbo ? buffer->map.length : buffer->size;
  }

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

void lovrBufferSetVertex(Buffer* buffer, int index, void* data) {
  memcpy((char*) buffer->data + index * buffer->stride, data, buffer->stride);

  glBindVertexArray(buffer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);

  int i;
  BufferAttribute attribute;
  size_t offset = 0;
  vec_foreach(&buffer->format, attribute, i) {
    glVertexAttribPointer(i, attribute.size, attribute.type, GL_FALSE, buffer->stride, (void*) offset);
    offset += attribute.size * sizeof(attribute.type);
  }
}

void lovrBufferSetVertices(Buffer* buffer, void* vertices, int size) {
  if (size > buffer->size * buffer->stride) {
    error("Buffer is not big enough");
  }

  glBindVertexArray(buffer->vao);
  memcpy(buffer->data, vertices, size);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * buffer->stride, buffer->data, buffer->usage);

  int i;
  BufferAttribute attribute;
  size_t offset = 0;
  vec_foreach(&buffer->format, attribute, i) {
    glVertexAttribPointer(i, attribute.size, attribute.type, GL_FALSE, buffer->stride, (void*) offset);
    offset += attribute.size * sizeof(attribute.type);
  }
}

unsigned int* lovrBufferGetVertexMap(Buffer* buffer, int* count) {
  *count = buffer->map.length;
  return buffer->map.data;
}

void lovrBufferSetVertexMap(Buffer* buffer, unsigned int* map, int count) {
  if (count == 0 || !map) {
    vec_clear(&buffer->map);
    glDeleteBuffers(1, &buffer->ibo);
    buffer->ibo = 0;
  } else if (!buffer->ibo) {
    glGenBuffers(1, &buffer->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
  }

  vec_clear(&buffer->map);
  vec_pusharr(&buffer->map, map, count);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), buffer->map.data, GL_STATIC_DRAW);
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

int lovrBufferSetDrawRange(Buffer* buffer, int rangeStart, int rangeCount) {
  if (rangeStart < 0 || rangeCount < 0 || rangeStart + rangeCount > buffer->size) {
    return 1;
  }

  buffer->rangeStart = rangeStart;
  buffer->rangeCount = rangeCount;

  return 0;
}

Texture* lovrBufferGetTexture(Buffer* buffer) {
  return buffer->texture;
}

void lovrBufferSetTexture(Buffer* buffer, Texture* texture) {
  buffer->texture = texture;
}
