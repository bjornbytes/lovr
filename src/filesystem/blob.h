#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  void* data;
  size_t size;
  const char* name;
  size_t seek;
} Blob;

Blob* lovrBlobCreate(void* data, size_t size, const char* name);
void lovrBlobDestroy(const Ref* ref);
