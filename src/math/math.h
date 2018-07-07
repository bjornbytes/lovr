#include "lib/vec/vec.h"
#include "math/randomGenerator.h"
#include <stdbool.h>

#pragma once

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))

typedef float* vec3;
typedef float* quat;
typedef float* mat4;
typedef vec_t(mat4) vec_mat4_t;

typedef struct {
  bool initialized;
  RandomGenerator* generator;
} MathState;

void lovrMathInit();
void lovrMathDestroy();
RandomGenerator* lovrMathGetRandomGenerator();
void lovrMathOrientationToDirection(float angle, float ax, float ay, float az, vec3 v);
float lovrMathGammaToLinear(float x);
float lovrMathLinearToGamma(float x);
float lovrMathNoise1(float x);
float lovrMathNoise2(float x, float y);
float lovrMathNoise3(float x, float y, float z);
float lovrMathNoise4(float x, float y, float z, float w);
