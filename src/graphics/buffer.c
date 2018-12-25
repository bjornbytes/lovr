#include "graphics/buffer.h"

size_t lovrBufferGetSize(Buffer* buffer) {
  return buffer->size;
}

BufferUsage lovrBufferGetUsage(Buffer* buffer) {
  return buffer->usage;
}
