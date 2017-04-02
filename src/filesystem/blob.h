#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  void* data;
  size_t size;
  const char* name;
} Blob;

Blob* lovrBlobCreate(void* data, size_t size, const char* name);
void lovrBlobDestroy(const Ref* ref);
