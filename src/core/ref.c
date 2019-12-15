#include "ref.h"
#include "util.h"
#include <stdlib.h>

void* _lovrAlloc(size_t size) {
  char* ref = calloc(1, sizeof(size_t) + size);
  lovrAssert(ref, "Out of memory");
  *((Ref*) ref) = 1;
  return ref + sizeof(size_t);
}
