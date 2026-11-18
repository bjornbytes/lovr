#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct Curve Curve;
typedef struct Mat4 Mat4;
typedef struct RandomGenerator RandomGenerator;

bool lovrMathInit(void);
void lovrMathDestroy(void);
float lovrMathGammaToLinear(float x);
float lovrMathLinearToGamma(float x);
double lovrMathNoise1(double x);
double lovrMathNoise2(double x, double y);
double lovrMathNoise3(double x, double y, double z);
double lovrMathNoise4(double x, double y, double z, double w);
RandomGenerator* lovrMathGetRandomGenerator(void);

// Curve

Curve* lovrCurveCreate(void);
void lovrCurveDestroy(void* ref);
bool lovrCurveEvaluate(Curve* curve, float t, float* point);
void lovrCurveGetTangent(Curve* curve, float t, float* point);
Curve* lovrCurveSlice(Curve* curve, float t1, float t2);
uint32_t lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, uint32_t index, float* point);
void lovrCurveSetPoint(Curve* curve, uint32_t index, float* point);
void lovrCurveAddPoint(Curve* curve, float* point, uint32_t index);
void lovrCurveRemovePoint(Curve* curve, uint32_t index);
float lovrCurveGetLength(Curve* curve, float t, uint32_t iterations);
float lovrCurveStep(Curve* curve, float distance, uint32_t iterations);

// Mat4

Mat4* lovrMat4Create(void);
void lovrMat4Destroy(void* ref);
float* lovrMat4GetData(Mat4* matrix);

// RandomGenerator

typedef union {
  uint64_t b64;
  struct {
    uint32_t lo;
    uint32_t hi;
  } b32;
} Seed;

RandomGenerator* lovrRandomGeneratorCreate(void);
void lovrRandomGeneratorDestroy(void* ref);
Seed lovrRandomGeneratorGetSeed(RandomGenerator* generator);
void lovrRandomGeneratorSetSeed(RandomGenerator* generator, Seed seed);
void lovrRandomGeneratorGetState(RandomGenerator* generator, char* state, size_t length);
int lovrRandomGeneratorSetState(RandomGenerator* generator, const char* state);
double lovrRandomGeneratorRandom(RandomGenerator* generator);
double lovrRandomGeneratorRandomNormal(RandomGenerator* generator);
