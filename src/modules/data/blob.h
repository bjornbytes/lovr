#include <stddef.h>
#include <stdint.h>

#pragma once

typedef struct Blob {
  uint32_t ref;
  void* data;
  size_t size;
  const char* name;
} Blob;

Blob* lovrBlobCreate(void* data, size_t size, const char* name);
void lovrBlobDestroy(void* ref);
