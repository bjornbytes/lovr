#include "data/blob.h"
#include "core/util.h"
#include <stdlib.h>

Blob* lovrBlobCreate(void* data, size_t size, const char* name) {
  Blob* blob = calloc(1, sizeof(Blob));
  lovrAssert(blob, "Out of memory");
  blob->ref = 1;
  blob->data = data;
  blob->size = size;
  blob->name = name;
  return blob;
}

void lovrBlobDestroy(void* ref) {
  Blob* blob = ref;
  free(blob->data);
  free(blob);
}
