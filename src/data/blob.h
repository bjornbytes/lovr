#include "util.h"
#include <stdlib.h>

#pragma once

typedef struct {
  Ref ref;
  void* data;
  size_t size;
  const char* name;
} Blob;

Blob* lovrBlobInit(Blob* blob, void* data, size_t size, const char* name);
#define lovrBlobCreate(...) lovrBlobInit(lovrAlloc(Blob), __VA_ARGS__)
void lovrBlobDestroy(void* ref);
