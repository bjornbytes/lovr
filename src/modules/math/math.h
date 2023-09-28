#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Image;

typedef struct Curve Curve;
typedef struct LightProbe LightProbe;
typedef struct Pool Pool;
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

// LightProbe

LightProbe* lovrLightProbeCreate(void);
void lovrLightProbeDestroy(void* ref);
void lovrLightProbeClear(LightProbe* probe);
void lovrLightProbeGetCoefficients(LightProbe* probe, float coefficients[9][3]);
void lovrLightProbeSetCoefficients(LightProbe* probe, float coefficients[9][3]);
void lovrLightProbeAddColor(LightProbe* probe, float color[3]);
void lovrLightProbeAddLight(LightProbe* probe, float direction[3], float color[3]);
void lovrLightProbeAddImage(LightProbe* probe, struct Image** images, uint32_t count);
void lovrLightProbeAddProbe(LightProbe* probe, LightProbe* other);
void lovrLightProbeLerp(LightProbe* probe, LightProbe* other, float t);
void lovrLightProbeScale(LightProbe* probe, float scale);

// Pool

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
    unsigned type : 4;
    unsigned generation : 4;
    unsigned index : 24;
    unsigned padding : 32;
  } handle;
} Vector;

Pool* lovrPoolCreate(void);
void lovrPoolDestroy(void* ref);
void lovrPoolGrow(Pool* pool, size_t count);
Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data);
float* lovrPoolResolve(Pool* pool, Vector vector);
void lovrPoolDrain(Pool* pool);

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
