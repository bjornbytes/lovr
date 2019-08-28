#include "arr.h"
#include "util.h"
#include <stdlib.h>

void _arr_reserve(void** data, size_t n, size_t* capacity, size_t stride) {
  if (*capacity == 0) {
    *capacity = 1;
  }

  while (*capacity < n) {
    *capacity <<= 1;
    lovrAssert(*capacity > 0, "Out of memory");
  }

  *data = realloc(*data, *capacity * stride);
  lovrAssert(*data, "Out of memory");
}
