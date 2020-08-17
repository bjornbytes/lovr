#include <stdbool.h>
#include <stddef.h>

#pragma once

typedef enum {
  BUFFER_VERTEX,
  BUFFER_INDEX,
  BUFFER_UNIFORM,
  BUFFER_SHADER_STORAGE,
  BUFFER_GENERIC,
  MAX_BUFFER_TYPES
} BufferType;

typedef enum {
  USAGE_STATIC,
  USAGE_DYNAMIC,
  USAGE_STREAM
} BufferUsage;

typedef struct Buffer Buffer;
Buffer* lovrBufferCreate(size_t size, void* data, BufferType type, BufferUsage usage, bool readable);
void lovrBufferDestroy(void* ref);
size_t lovrBufferGetSize(Buffer* buffer);
bool lovrBufferIsReadable(Buffer* buffer);
BufferUsage lovrBufferGetUsage(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, size_t offset, bool unsynchronized);
void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size);
void lovrBufferUnmap(Buffer* buffer);
void lovrBufferDiscard(Buffer* buffer);
