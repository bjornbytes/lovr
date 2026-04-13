#include "util.h"
#include <stddef.h>
#include <stdint.h>

#pragma once

typedef struct Blob {
  lovr_atomic_uint ref;
  void* data;
  size_t size;
  char* name;
  struct Blob* root;
} Blob;

Blob* lovrBlobCreate(void* data, size_t size, const char* name);
Blob* lovrBlobCreateView(Blob* blob, size_t offset, size_t size, const char* name);
void lovrBlobDestroy(void* ref);
