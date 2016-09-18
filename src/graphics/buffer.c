#include "buffer.h"
#include "../util.h"
#include <stdlib.h>

void lovrBufferDestroy(Buffer* buffer) {
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteVertexArrays(1, &buffer->vao);
  vec_deinit(&buffer->map);
  free(buffer->data);
  free(buffer);
}

void lovrBufferDraw(Buffer* buffer) {
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);
    glDrawElements(buffer->drawMode, count, GL_UNSIGNED_INT, (GLvoid*) NULL + start);
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
  vec_reserve(&buffer->map, count);
  vec_pusharr(&buffer->map, map, count);

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), buffer->map.data, GL_STATIC_DRAW);
}

char lovrBufferIsRangeEnabled(Buffer* buffer) {
  return buffer->isRangeEnabled;
}

void lovrBufferSetRangeEnabled(Buffer* buffer, char isEnabled) {
  buffer->isRangeEnabled = isEnabled;
}

void lovrBufferGetDrawRange(Buffer* buffer, int* start, int* end) {
  *start = buffer->rangeStart;
  *end = buffer->rangeStart + buffer->rangeCount - 1;
}

int lovrBufferSetDrawRange(Buffer* buffer, int rangeStart, int rangeEnd) {
  if (rangeStart < 0 || rangeEnd < 0 || rangeStart > rangeEnd) {
    return 1;
  }

  buffer->rangeStart = rangeStart;
  buffer->rangeCount = rangeEnd - rangeStart + 1;
  return 0;
}
