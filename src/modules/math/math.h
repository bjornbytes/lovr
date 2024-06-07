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
void lovrCurveEvaluate(Curve* curve, float t, float* point);
void lovrCurveGetTangent(Curve* curve, float t, float* point);
Curve* lovrCurveSlice(Curve* curve, float t1, float t2);
size_t lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, size_t index, float* point);
void lovrCurveSetPoint(Curve* curve, size_t index, float* point);
void lovrCurveAddPoint(Curve* curve, float* point, size_t index);
void lovrCurveRemovePoint(Curve* curve, size_t index);

// Mat4

Mat4* lovrMat4Create(void);
void lovrMat4Destroy(void* ref);
Mat4* lovrMat4Clone(Mat4* matrix);
float* lovrMat4GetPointer(Mat4* matrix);
bool lovrMat4Equals(Mat4* matrix, Mat4* other);
void lovrMat4GetPosition(Mat4* matrix, float* position);
void lovrMat4GetOrientation(Mat4* matrix, float* orientation);
void lovrMat4GetAngleAxis(Mat4* matrix, float* angle, float* ax, float* ay, float* az);
void lovrMat4GetScale(Mat4* matrix, float* scale);
void lovrMat4Identity(Mat4* matrix);
void lovrMat4Invert(Mat4* matrix);
void lovrMat4Transpose(Mat4* matrix);
void lovrMat4Translate(Mat4* matrix, float* translation);
void lovrMat4Rotate(Mat4* matrix, float* rotation);
void lovrMat4Scale(Mat4* matrix, float* scale);

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
