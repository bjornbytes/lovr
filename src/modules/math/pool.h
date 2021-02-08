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

typedef union {
  void* pointer;
  struct {
    uint8_t type;
    uint8_t generation;
    uint16_t index;
    uint32_t padding;
  } handle;
} Vector;

typedef struct Pool Pool;
Pool* lovrPoolCreate(void);
void lovrPoolDestroy(void* ref);
void lovrPoolGrow(Pool* pool, size_t count);
Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data);
float* lovrPoolResolve(Pool* pool, Vector vector);
void lovrPoolDrain(Pool* pool);
