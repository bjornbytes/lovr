#include "ref.h"
#include "util.h"
#include <stdlib.h>

void* _lovrAlloc(size_t size) {
  Ref* ref = calloc(1, sizeof(Ref) + size);
  lovrAssert(ref, "Out of memory");
  return *ref = 1, ref + 1;
}
