#include "math/randomGenerator.h"
#include <stdbool.h>

#pragma once

typedef struct {
  bool initialized;
  RandomGenerator* generator;
} MathState;

bool lovrMathInit(void);
void lovrMathDestroy(void);
RandomGenerator* lovrMathGetRandomGenerator(void);
void lovrMathOrientationToDirection(float angle, float ax, float ay, float az, float* v);
float lovrMathGammaToLinear(float x);
float lovrMathLinearToGamma(float x);
float lovrMathNoise1(float x);
float lovrMathNoise2(float x, float y);
float lovrMathNoise3(float x, float y, float z);
float lovrMathNoise4(float x, float y, float z, float w);
