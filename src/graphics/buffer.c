#include "buffer.h"
#include "../util.h"
#include <stdlib.h>

void lovrBufferDestroy(Buffer* buffer) {
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteVertexArrays(1, &buffer->vao);
  free(buffer->data);
  free(buffer);
}

void lovrBufferDraw(Buffer* buffer) {
  glBindVertexArray(buffer->vao);
  glEnableVertexAttribArray(0);
  glDrawArrays(buffer->drawMode, buffer->rangeStart, buffer->rangeCount);
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

void lovrBufferGetDrawRange(Buffer* buffer, int* start, int* end) {
  *start = buffer->rangeStart + 1;
  *end = buffer->rangeStart + 1 + buffer->rangeCount;
}

int lovrBufferSetDrawRange(Buffer* buffer, int rangeStart, int rangeEnd) {
  if (rangeStart <= 0 || rangeEnd <= 0 || rangeStart > rangeEnd) {
    return 1;
  }

  buffer->rangeStart = rangeStart - 1;
  buffer->rangeCount = rangeEnd - rangeStart + 1;
  return 0;
}
