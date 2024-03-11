#include "data/blob.h"
#include "util.h"
#include <stdlib.h>

Blob* lovrBlobCreate(void* data, size_t size, const char* name) {
  Blob* blob = lovrMalloc(sizeof(Blob));
  blob->ref = 1;
  blob->data = data;
  blob->size = size;
  blob->name = name;
  return blob;
}

void lovrBlobDestroy(void* ref) {
  Blob* blob = ref;
  lovrFree(blob->data);
  lovrFree(blob);
}
