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
