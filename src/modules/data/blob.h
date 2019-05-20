#include <stddef.h>

#pragma once

typedef struct Blob {
  void* data;
  size_t size;
  const char* name;
} Blob;

Blob* lovrBlobInit(Blob* blob, void* data, size_t size, const char* name);
#define lovrBlobCreate(...) lovrBlobInit(lovrAlloc(Blob), __VA_ARGS__)
void lovrBlobDestroy(void* ref);
