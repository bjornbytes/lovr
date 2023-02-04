#include "math/pool.h"
#include "util.h"
#include <stdlib.h>

static const size_t vectorComponents[] = {
  [V_VEC2] = 2,
  [V_VEC3] = 4,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

struct Pool {
  uint32_t ref;
  float* data;
  uint32_t count;
  uint32_t cursor;
  uint32_t generation;
};

Pool* lovrPoolCreate() {
  Pool* pool = calloc(1, sizeof(Pool));
  lovrAssert(pool, "Out of memory");
  pool->ref = 1;
  lovrPoolGrow(pool, 1 << 12);
  return pool;
}

void lovrPoolDestroy(void* ref) {
  Pool* pool = ref;
  free(pool->data);
  free(pool);
}

void lovrPoolGrow(Pool* pool, size_t count) {
  lovrAssert(count <= (1 << 24), "Temporary vector space exhausted.  Try using lovr.math.drain to drain the vector pool periodically.");
  pool->count = count;
  pool->data = realloc(pool->data, pool->count * sizeof(float));
  lovrAssert(pool->data, "Out of memory");
}

Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data) {
  lovrCheck(pool, "The math module must be initialized to create vectors");

  size_t count = vectorComponents[type];

  if (pool->cursor + count > pool->count) {
    lovrPoolGrow(pool, pool->count * 2);
  }

  Vector v = {
    .handle = {
      .type = type,
      .generation = pool->generation,
      .index = pool->cursor
    }
  };

  *data = pool->data + pool->cursor;
  pool->cursor += count;
  return v;
}

float* lovrPoolResolve(Pool* pool, Vector vector) {
  lovrAssert(vector.handle.generation == pool->generation, "Attempt to use a temporary vector from a previous frame");
  return pool->data + vector.handle.index;
}

void lovrPoolDrain(Pool* pool) {
  pool->cursor = 0;
  pool->generation = (pool->generation + 1) & 0xf;
}
