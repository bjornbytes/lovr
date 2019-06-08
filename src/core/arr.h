#include <stddef.h>

#pragma once

// A dynamic array that uses the stack initially and switches to the heap if it gets big enough.

#define arr_t(T, n)\
  struct { T *data; T temp[n]; size_t length, capacity; }

#define arr_init(a)\
  (a)->data = (a)->temp,\
  (a)->length = 0,\
  (a)->capacity = sizeof((a)->temp) / sizeof((a)->temp[0])

#define arr_free(a)\
  if ((a)->data != (a)->temp) free((a)->data)

#define arr_reserve(a, n)\
  n > (a)->capacity ?\
    _arr_reserve((void**) &((a)->data), (a)->temp, n, &(a)->capacity, sizeof(*(a)->data)) :\
    (void) 0

#define arr_push(a, x)\
  arr_reserve(a, (a)->length + 1),\
  (a)->data[(a)->length++] = x

#define arr_pop(a)\
  (a)->data[--(a)->length]

#define arr_append(a, p, n)\
  arr_reserve(a, (a)->length + n),\
  memcpy((a)->data + (a)->length, p, n * sizeof(*(p))),\
  (a)->length += n

#define arr_splice(a, i, n)\
  memmove((a)->data + i, (a)->data + (i + n), n * sizeof(*(a)->data)),\
  (a)->length -= n

#define arr_clear(a)\
  (a)->length = 0

void _arr_reserve(void** data, void* temp, size_t n, size_t* capacity, size_t stride);
