#include <stdbool.h>

#pragma once

struct RandomGenerator;

bool lovrMathInit(void);
void lovrMathDestroy(void);
struct RandomGenerator* lovrMathGetRandomGenerator(void);
float lovrMathGammaToLinear(float x);
float lovrMathLinearToGamma(float x);
double lovrMathNoise1(double x);
double lovrMathNoise2(double x, double y);
double lovrMathNoise3(double x, double y, double z);
double lovrMathNoise4(double x, double y, double z, double w);
