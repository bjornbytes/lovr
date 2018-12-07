#include <stdlib.h>

#pragma once

typedef enum {
  USAGE_STATIC,
  USAGE_DYNAMIC,
  USAGE_STREAM
} BufferUsage;

typedef struct Buffer Buffer;

Buffer* lovrBufferCreate(size_t size, void* data, BufferUsage usage);
void lovrBufferDestroy(void* ref);
size_t lovrBufferGetSize(Buffer* buffer);
BufferUsage lovrBufferGetUsage(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, size_t offset);
void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size);
void lovrBufferLock(Buffer* buffer);
void lovrBufferUnlock(Buffer* buffer);
