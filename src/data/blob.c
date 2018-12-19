#include "data/blob.h"

Blob* lovrBlobInit(Blob* blob, void* data, size_t size, const char* name) {
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
