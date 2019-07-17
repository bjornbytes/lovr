#include "math/pool.h"
#include <stdlib.h>

#define POOL_ALIGN 16

static const size_t vectorComponents[] = {
  [V_VEC2] = 2,
  [V_VEC3] = 4,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

Pool* lovrPoolInit(Pool* pool) {
  lovrPoolGrow(pool, 1 << 12);
  return pool;
}

void lovrPoolDestroy(void* ref) {
  Pool* pool = ref;
  free(pool->data);
}

void lovrPoolGrow(Pool* pool, size_t count) {
  lovrAssert(count <= (1 << 16), "Congratulations!  You have run out of memory for vectors.  Try using lovr.math.drain to drain the vector Pool periodically."); // Only 24 bits for vector handles
  pool->count = count;
  pool->data = realloc(pool->data, pool->count * sizeof(float));
  lovrAssert(pool->data, "Out of memory");
  pool->floats = (float*) ALIGN((uint8_t*) pool->data + POOL_ALIGN - 1, POOL_ALIGN);
}

Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data) {
  size_t count = vectorComponents[type];
  if (pool->cursor + count > pool->count - 4) { // Leave 4 floats of padding for alignment adjustment
    lovrPoolGrow(pool, pool->count * 2);
  }
  Vector v = { type, pool->generation, pool->cursor };
  if (data) {
    *data = pool->floats + pool->cursor;
  }
  pool->cursor += count;
  return v;
}

float* lovrPoolResolve(Pool* pool, Vector vector) {
  lovrAssert(vector.generation == pool->generation, "Attempt to use a vector in a different generation than the one it was created in (vectors can not be saved into variables)");
  return pool->floats + vector.index;
}

void lovrPoolDrain(Pool* pool) {
  pool->cursor = 0;
  pool->generation = (pool->generation + 1) & 0xff;
}
