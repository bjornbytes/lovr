#include "ref.h"
#include "util.h"
#include <stdlib.h>

void* _lovrAlloc(size_t size) {
  char* ref = calloc(1, sizeof(RefHeader) + size);
  lovrAssert(ref, "Out of memory");
  ((RefHeader*)ref)->ref = 1;
  
  return ref + sizeof(size_t);
}
