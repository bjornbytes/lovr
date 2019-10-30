#include <stddef.h>

#pragma once

#define arr_t(T)\
  struct { T* data; size_t length, capacity; }

#define arr_init(a)\
  (a)->data = NULL,\
  (a)->length = 0,\
  (a)->capacity = 0

#define arr_free(a)\
  free((a)->data)

#define arr_reserve(a, n)\
  n > (a)->capacity ?\
    _arr_reserve((void**) &((a)->data), n, &(a)->capacity, sizeof(*(a)->data)) :\
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

void _arr_reserve(void** data, size_t n, size_t* capacity, size_t stride);
