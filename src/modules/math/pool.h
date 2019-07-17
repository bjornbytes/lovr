#include "util.h"
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef enum {
  V_NONE,
  V_VEC2,
  V_VEC3,
  V_VEC4,
  V_QUAT,
  V_MAT4,
  MAX_VECTOR_TYPES
} VectorType;

typedef struct {
  uint8_t type;
  uint8_t generation;
  uint16_t index;
} Vector;

typedef struct Pool {
  void* data;
  float* floats;
  size_t count;
  size_t cursor;
  size_t generation;
} Pool;

Pool* lovrPoolInit(Pool* pool);
#define lovrPoolCreate(...) lovrPoolInit(lovrAlloc(Pool))
void lovrPoolDestroy(void* ref);
LOVR_EXPORT void lovrPoolGrow(Pool* pool, size_t count);
Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data);
float* lovrPoolResolve(Pool* pool, Vector vector);
void lovrPoolDrain(Pool* pool);
