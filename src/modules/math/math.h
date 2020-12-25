#include <stdbool.h>

#pragma once

struct RandomGenerator;

bool lovrMathInit(void);
void lovrMathDestroy(void);
struct RandomGenerator* lovrMathGetRandomGenerator(void);
float lovrMathGammaToLinear(float x);
float lovrMathLinearToGamma(float x);
float lovrMathNoise1(float x);
float lovrMathNoise2(float x, float y);
float lovrMathNoise3(float x, float y, float z);
float lovrMathNoise4(float x, float y, float z, float w);
