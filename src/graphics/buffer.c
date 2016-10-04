#include "buffer.h"
#include "graphics.h"
#include <stdlib.h>

Buffer* lovrBufferCreate(int size, BufferDrawMode drawMode, BufferUsage usage) {
  Buffer* buffer = malloc(sizeof(Buffer));

  buffer->drawMode = drawMode;
  buffer->usage = usage;
  buffer->size = size;
  buffer->data = malloc(buffer->size * 3 * sizeof(GLfloat));
  buffer->vao = 0;
  buffer->vbo = 0;
  buffer->ibo = 0;
  buffer->isRangeEnabled = 0;
  buffer->rangeStart = 0;
  buffer->rangeCount = buffer->size;

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * 3 * sizeof(GLfloat), buffer->data, buffer->usage);

  glGenVertexArrays(1, &buffer->vao);

  vec_init(&buffer->map);

  return buffer;
}

void lovrBufferDestroy(Buffer* buffer) {
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteVertexArrays(1, &buffer->vao);
  vec_deinit(&buffer->map);
  free(buffer->data);
  free(buffer);
}

void lovrBufferDraw(Buffer* buffer) {
  lovrGraphicsPrepare();
  glBindVertexArray(buffer->vao);
  glEnableVertexAttribArray(0);
  int usingIbo = buffer->map.length > 0;

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
  glDisableVertexAttribArray(0);
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

void lovrBufferGetVertex(Buffer* buffer, int index, float* x, float* y, float* z) {
  *x = buffer->data[3 * index + 0];
  *y = buffer->data[3 * index + 1];
  *z = buffer->data[3 * index + 2];
}

void lovrBufferSetVertex(Buffer* buffer, int index, float x, float y, float z) {
  buffer->data[3 * index + 0] = x;
  buffer->data[3 * index + 1] = y;
  buffer->data[3 * index + 2] = z;

  glBindVertexArray(buffer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * 3 * sizeof(GLfloat), buffer->data, buffer->usage);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
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
