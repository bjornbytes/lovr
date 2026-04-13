#include "data/blob.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

Blob* lovrBlobCreate(void* data, size_t size, const char* name) {
  Blob* blob = lovrCalloc(sizeof(Blob));
  SDL_SetAtomicU32(&blob->ref, 1);
  blob->data = data;
  blob->size = size;
  blob->name = lovrStrdup(name);
  blob->root = blob;
  return blob;
}

Blob* lovrBlobCreateView(Blob* parent, size_t offset, size_t size, const char* name) {
  Blob* blob = lovrCalloc(sizeof(Blob));
  SDL_SetAtomicU32(&blob->ref, 1);
  blob->data = (char*) parent->data + offset;
  blob->size = size;
  blob->name = lovrStrdup(name);
  blob->root = parent->root;
  lovrRetain(blob->root);
  return blob;
}

void lovrBlobDestroy(void* ref) {
  Blob* blob = ref;
  if (blob->root != blob) {
    lovrRelease(blob->root, lovrBlobDestroy);
  } else {
    lovrFree(blob->data);
  }
  lovrFree(blob->name);
  lovrFree(blob);
}
