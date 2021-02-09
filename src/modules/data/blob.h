#include <stddef.h>
#include "core/util.h"

#pragma once

typedef struct Blob {
  ref_t ref;
  void* data;
  size_t size;
  const char* name;
} Blob;

Blob* lovrBlobCreate(void* data, size_t size, const char* name);
void lovrBlobDestroy(void* ref);
