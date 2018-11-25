#include "math/pool.h"
#include <stdlib.h>

static const size_t sizeOfMathType[] = {
  [MATH_VEC3] = 4 * sizeof(float),
  [MATH_QUAT] = 4 * sizeof(float),
  [MATH_MAT4] = 16 * sizeof(float)
};

Pool* lovrPoolCreate(size_t size) {
  Pool* pool = lovrAlloc(Pool, lovrPoolDestroy);
  if (!pool) return NULL;

  pool->size = size;
  pool->data = calloc(1, size + POOL_ALIGN - 1);
  pool->head = (uint8_t*) ALIGN((uint8_t*) pool->data + POOL_ALIGN - 1, POOL_ALIGN);
  lovrAssert(pool->data, "Could not allocate Pool memory");
  pool->usage = 0;

  return pool;
}

void lovrPoolDestroy(void* ref) {
  Pool* pool = ref;
  free(pool->data);
  free(pool);
}

float* lovrPoolAllocate(Pool* pool, MathType type) {
  size_t size = sizeOfMathType[type];
  if (pool->usage + size > pool->size) return NULL;
  float* p = (float*) (pool->head + pool->usage);
  pool->usage += size;
  return p;
}

void lovrPoolDrain(Pool* pool) {
  pool->usage = 0;
}

size_t lovrPoolGetSize(Pool* pool) {
  return pool->size;
}

size_t lovrPoolGetUsage(Pool* pool) {
  return pool->usage;
}

float* lovrPoolAllocateVec3(Pool* pool) { return lovrPoolAllocate(pool, MATH_VEC3); }
float* lovrPoolAllocateQuat(Pool* pool) { return lovrPoolAllocate(pool, MATH_QUAT); }
float* lovrPoolAllocateMat4(Pool* pool) { return lovrPoolAllocate(pool, MATH_MAT4); }
