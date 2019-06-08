#include "arr.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

void _arr_reserve(void** data, void* temp, size_t n, size_t* capacity, size_t stride) {
  size_t oldCapacity = *capacity;

  while (*capacity < n) {
    *capacity <<= 1;
    lovrAssert(*capacity > 0, "Out of memory");
  }

  if (*data == temp) {
    *data = realloc(NULL, *capacity * stride);
    lovrAssert(*data, "Out of memory");
    memcpy(*data, temp, oldCapacity * stride);
  } else {
    *data = realloc(*data, *capacity * stride);
    lovrAssert(*data, "Out of memory");
  }
}
