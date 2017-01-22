#include "vendor/vec/vec.h"

#ifndef LOVR_MATH_TYPES
#define LOVR_MATH_TYPES

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

typedef float* vec3;
typedef float* quat;
typedef float* mat4;
typedef vec_t(mat4) vec_mat4_t;

#endif
