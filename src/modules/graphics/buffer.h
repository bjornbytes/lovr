#include "graphics/opengl.h"
#include <stddef.h>
#include <stdbool.h>

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

typedef struct Buffer {
  void* data;
  size_t size;
  size_t flushFrom;
  size_t flushTo;
  BufferType type;
  BufferUsage usage;
  bool mapped;
  bool readable;
  GPU_BUFFER_FIELDS
} Buffer;

Buffer* lovrBufferInit(Buffer* buffer, size_t size, void* data, BufferType type, BufferUsage usage, bool readable);
#define lovrBufferCreate(...) lovrBufferInit(lovrAlloc(Buffer), __VA_ARGS__)
void lovrBufferDestroy(void* ref);
size_t lovrBufferGetSize(Buffer* buffer);
bool lovrBufferIsReadable(Buffer* buffer);
BufferUsage lovrBufferGetUsage(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, size_t offset);
void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size);
void lovrBufferUnmap(Buffer* buffer);
void lovrBufferDiscard(Buffer* buffer);
