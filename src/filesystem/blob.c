#include "filesystem/blob.h"

Blob* lovrBlobCreate(void* data, size_t size, const char* name) {
  Blob* blob = lovrAlloc(sizeof(Blob), lovrBlobDestroy);
  if (!blob) return NULL;

  blob->data = data;
  blob->size = size;
  blob->name = name;
  blob->seek = 0;

  return blob;
}

void lovrBlobDestroy(const Ref* ref) {
  Blob* blob = containerof(ref, Blob);
  free(blob->data);
  free(blob);
}
