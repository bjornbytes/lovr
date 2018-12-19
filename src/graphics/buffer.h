#include "graphics/opengl.h"
#include <stdlib.h>

#pragma once

typedef enum {
  USAGE_STATIC,
  USAGE_DYNAMIC,
  USAGE_STREAM
} BufferUsage;

typedef struct {
  Ref ref;
  void* data;
  size_t size;
  BufferUsage usage;
  GPU_BUFFER_FIELDS
} Buffer;

Buffer* lovrBufferInit(Buffer* buffer, size_t size, void* data, BufferUsage usage, bool readable);
#define lovrBufferCreate(...) lovrBufferInit(lovrAlloc(Buffer), __VA_ARGS__)
void lovrBufferDestroy(void* ref);
size_t lovrBufferGetSize(Buffer* buffer);
BufferUsage lovrBufferGetUsage(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, size_t offset);
void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size);
void lovrBufferLock(Buffer* buffer);
void lovrBufferUnlock(Buffer* buffer);
