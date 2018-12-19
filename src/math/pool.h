#include "util.h"

#define POOL_ALIGN 16
#define DEFAULT_POOL_SIZE (640 * 1024)

#pragma once

typedef enum {
  MATH_VEC3,
  MATH_QUAT,
  MATH_MAT4,
  MAX_MATH_TYPES
} MathType;

typedef struct {
  Ref ref;
  float* data;
  size_t size;
  size_t usage;
  uint8_t* head;
} Pool;

Pool* lovrPoolInit(Pool* pool, size_t size);
#define lovrPoolCreate(...) lovrPoolInit(lovrAlloc(Pool, lovrPoolDestroy), __VA_ARGS__)
void lovrPoolDestroy(void* ref);
float* lovrPoolAllocate(Pool* pool, MathType type);
void lovrPoolDrain(Pool* pool);
size_t lovrPoolGetSize(Pool* pool);
size_t lovrPoolGetUsage(Pool* pool);

// For you, LuaJIT
float* lovrPoolNewVec3(Pool* pool);
float* lovrPoolNewQuat(Pool* pool);
float* lovrPoolNewMat4(Pool* pool);
