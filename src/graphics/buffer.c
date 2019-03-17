#include "graphics/buffer.h"

size_t lovrBufferGetSize(Buffer* buffer) {
  return buffer->size;
}

bool lovrBufferIsReadable(Buffer* buffer) {
  return buffer->readable;
}

BufferUsage lovrBufferGetUsage(Buffer* buffer) {
  return buffer->usage;
}

void lovrBufferMarkRange(Buffer* buffer, size_t offset, size_t size) {
  size_t end = offset + size;
  buffer->flushFrom = MIN(buffer->flushFrom, offset);
  buffer->flushTo = MAX(buffer->flushTo, end);
}

void lovrBufferFlush(Buffer* buffer) {
  if (buffer->flushTo <= buffer->flushFrom) {
    return;
  }

  lovrBufferFlushRange(buffer, buffer->flushFrom, buffer->flushTo - buffer->flushFrom);
  buffer->flushFrom = SIZE_MAX;
  buffer->flushTo = 0;
}
