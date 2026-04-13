#include "math.h"
#include "core/maf.h"
#include "util.h"
#include "lib/noise/simplexnoise1234.h"
#include <math.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct Curve {
  lovr_atomic_uint ref;
  arr_t(float) points;
};

struct Mat4 {
  lovr_atomic_uint ref;
  float m[16];
};

struct RandomGenerator {
  lovr_atomic_uint ref;
  Seed seed;
  Seed state;
  double lastRandomNormal;
};

static lovr_atomic_uint ref;

static struct {
  RandomGenerator* generator;
} state;

bool lovrMathInit(void) {
  if (!lovrModuleAcquire(&ref)) return true;
  state.generator = lovrRandomGeneratorCreate();
  Seed seed = { .b64 = (uint64_t) time(0) };
  lovrRandomGeneratorSetSeed(state.generator, seed);
  lovrModuleReady(&ref);
  return true;
}

void lovrMathDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  lovrRelease(state.generator, lovrRandomGeneratorDestroy);
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

float lovrMathGammaToLinear(float x) {
  if (x <= .04045f) {
    return x / 12.92f;
  } else {
    return powf((x + .055f) / 1.055f, 2.4f);
  }
}

float lovrMathLinearToGamma(float x) {
  if (x <= .0031308f) {
    return x * 12.92f;
  } else {
    return 1.055f * powf(x, 1.f / 2.4f) - .055f;
  }
}

double lovrMathNoise1(double x) {
  return snoise1(x) * .5 + .5;
}

double lovrMathNoise2(double x, double y) {
  return snoise2(x, y) * .5 + .5;
}

double lovrMathNoise3(double x, double y, double z) {
  return snoise3(x, y, z) * .5 + .5;
}

double lovrMathNoise4(double x, double y, double z, double w) {
  return snoise4(x, y, z, w) * .5 + .5;
}

RandomGenerator* lovrMathGetRandomGenerator(void) {
  return state.generator;
}

// Curve

// Explicit curve evaluation, unroll simple cases to avoid pow overhead
static void evaluate(float* restrict P, size_t n, float t, float* p) {
  if (n == 2) {
    p[0] = P[0] + (P[4] - P[0]) * t;
    p[1] = P[1] + (P[5] - P[1]) * t;
    p[2] = P[2] + (P[6] - P[2]) * t;
    p[3] = P[3] + (P[7] - P[3]) * t;
  } else if (n == 3) {
    float t1 = (1.f - t);
    float a = t1 * t1;
    float b = 2.f * t1 * t;
    float c = t * t;
    p[0] = a * P[0] + b * P[4] + c * P[8];
    p[1] = a * P[1] + b * P[5] + c * P[9];
    p[2] = a * P[2] + b * P[6] + c * P[10];
    p[3] = a * P[3] + b * P[7] + c * P[11];
  } else if (n == 4) {
    float t1 = (1.f - t);
    float a = t1 * t1 * t1;
    float b = 3.f * t1 * t1 * t;
    float c = 3.f * t1 * t * t;
    float d = t * t * t;
    p[0] = a * P[0] + b * P[4] + c * P[8] + d * P[12];
    p[1] = a * P[1] + b * P[5] + c * P[9] + d * P[13];
    p[2] = a * P[2] + b * P[6] + c * P[10] + d * P[14];
    p[3] = a * P[3] + b * P[7] + c * P[11] + d * P[15];
  } else {
    float b = 1.f;
    p[0] = p[1] = p[2] = p[3] = 0.f;
    for (size_t i = 0; i < n; i++, b *= (float) (n - i) / i) {
      float c1 = powf(1.f - t, n - (i + 1));
      float c2 = powf(t, i);
      p[0] += b * c1 * c2 * P[i * 4 + 0];
      p[1] += b * c1 * c2 * P[i * 4 + 1];
      p[2] += b * c1 * c2 * P[i * 4 + 2];
      p[3] += b * c1 * c2 * P[i * 4 + 3];
    }
  }
}

Curve* lovrCurveCreate(void) {
  Curve* curve = lovrCalloc(sizeof(Curve));
lovr_atomic_store(&curve->ref, 1);
  arr_init(&curve->points);
  arr_reserve(&curve->points, 16);
  return curve;
}

void lovrCurveDestroy(void* ref) {
  Curve* curve = ref;
  arr_free(&curve->points);
  lovrFree(curve);
}

bool lovrCurveEvaluate(Curve* curve, float t, float* p) {
  lovrCheck(curve->points.length >= 8, "Need at least 2 points to evaluate a Curve");
  lovrCheck(t >= 0.f && t <= 1.f, "Curve evaluation interval must be within [0, 1]");
  evaluate(curve->points.data, curve->points.length / 4, t, p);
  return true;
}

void lovrCurveGetTangent(Curve* curve, float t, float* p) {
  float q[4];
  size_t n = curve->points.length / 4;
  evaluate(curve->points.data, n - 1, t, q);
  evaluate(curve->points.data + 4, n - 1, t, p);
  p[0] -= q[0];
  p[1] -= q[1];
  p[2] -= q[2];
  p[3] -= q[3];
  vec3_normalize(p);
}

Curve* lovrCurveSlice(Curve* curve, float t1, float t2) {
  lovrCheck(curve->points.length >= 8, "Need at least 2 points to slice a Curve");
  lovrCheck(t1 >= 0.f && t2 <= 1.f, "Curve slice interval must be within [0, 1]");

  Curve* new = lovrCurveCreate();
  arr_reserve(&new->points, curve->points.length);
  new->points.length = curve->points.length;

  size_t n = curve->points.length / 4;

  // Right half of split at t1
  for (size_t i = 0; i < n - 1; i++) {
    evaluate(curve->points.data + 4 * i, n - i, t1, new->points.data + 4 * i);
  }

  memcpy(new->points.data + 4 * (n - 1), curve->points.data + 4 * (n - 1), 4 * sizeof(float));

  // Split segment at t2, taking left half
  float t = (t2 - t1) / (1.f - t1);
  for (size_t i = n - 1; i >= 1; i--) {
    float point[4];
    evaluate(new->points.data, i + 1, t, point);
    memcpy(new->points.data + 4 * i, point, 4 * sizeof(float));
  }

  return new;
}

size_t lovrCurveGetPointCount(Curve* curve) {
  return curve->points.length / 4;
}

void lovrCurveGetPoint(Curve* curve, size_t index, float* point) {
  memcpy(point, curve->points.data + 4 * index, 4 * sizeof(float));
}

void lovrCurveSetPoint(Curve* curve, size_t index, float* point) {
  memcpy(curve->points.data + 4 * index, point, 4 * sizeof(float));
}

void lovrCurveAddPoint(Curve* curve, float* point, size_t index) {

  // Reserve enough memory for 4 more floats, then record destination once memory is allocated
  arr_reserve(&curve->points, curve->points.length + 4);
  float* dest = curve->points.data + index * 4;

  // Shift remaining points over (if any) to create empty space
  if (index * 4 != curve->points.length) {
    memmove(dest + 4, dest, (curve->points.length - index * 4) * sizeof(float));
  }

  // Fill the empty space with the new point
  curve->points.length += 4;
  memcpy(dest, point, 4 * sizeof(float));
}

void lovrCurveRemovePoint(Curve* curve, size_t index) {
  arr_splice(&curve->points, index * 4, 4);
}

static void evaluateDerivative(float* restrict P, size_t n, float t, float* dp) {
  if (n < 2) {
    dp[0] = dp[1] = dp[2] = dp[3] = 0.f;
    return;
  }
  float* points = lovrMalloc((n - 1) * 4 * sizeof(float));
  // Difference points for derivative
  for (size_t i = 0; i < n - 1; i++) {
    points[i * 4 + 0] = (float)(n - 1) * (P[(i + 1) * 4 + 0] - P[i * 4 + 0]);
    points[i * 4 + 1] = (float)(n - 1) * (P[(i + 1) * 4 + 1] - P[i * 4 + 1]);
    points[i * 4 + 2] = (float)(n - 1) * (P[(i + 1) * 4 + 2] - P[i * 4 + 2]);
    points[i * 4 + 3] = (float)(n - 1) * (P[(i + 1) * 4 + 3] - P[i * 4 + 3]);
  }
  evaluate(points, n - 1, t, dp);
  lovrFree(points);
}

float lovrCurveGetLength(Curve* curve, float t, int iterations) {
  t = CLAMP(t, 0.f, 1.f);
  if (t == 0.f) return 0.f;
  size_t n = curve->points.length / 4;
  float length = 0.f;
  // Numerical integration using adaptive Simpson's rule:
  //   length = (f(b) - f(a)) / 6 * (f(a) + 4 * f((a + b) / 2) + f(b))
  float dt = t / iterations;
  for (int i = 0; i < iterations; i++) {
    float t0 = i * dt;
    float t1 = t0 + dt * 0.5f;
    float t2 = t0 + dt;
    float dp0[4], dp1[4], dp2[4];
    evaluateDerivative(curve->points.data, n, t0, dp0);
    evaluateDerivative(curve->points.data, n, t1, dp1);
    evaluateDerivative(curve->points.data, n, t2, dp2);
    float speed0 = sqrtf(dp0[0] * dp0[0] + dp0[1] * dp0[1] + dp0[2] * dp0[2]);
    float speed1 = sqrtf(dp1[0] * dp1[0] + dp1[1] * dp1[1] + dp1[2] * dp1[2]);
    float speed2 = sqrtf(dp2[0] * dp2[0] + dp2[1] * dp2[1] + dp2[2] * dp2[2]);
    length += (dt / 6.f) * (speed0 + 4.f * speed1 + speed2);
  }
  return length;
}

float lovrCurveStep(Curve* curve, float distance, int iterations) {
  if (distance <= 0.f) return 0.f;
  // Binary search for t
  float tMin = 0.f;
  float tMax = 1.f;
  const float epsilon = 1e-6f;
  for (int i = 0; i < iterations; i++) {
    float tMid = (tMin + tMax) * 0.5f;
    float lengthAtT = lovrCurveGetLength(curve, tMid, iterations);
    if (fabsf(lengthAtT - distance) < epsilon) {
      return tMid;
    }
    if (lengthAtT < distance) {
      tMin = tMid;
    } else {
      tMax = tMid;
    }
    if (tMax - tMin < epsilon) {
      return (tMin + tMax) * 0.5f;
    }
  }
  return (tMin + tMax) * 0.5f;
}

// Mat4

Mat4* lovrMat4Create(void) {
  Mat4* matrix = lovrMalloc(sizeof(Mat4));
lovr_atomic_store(&matrix->ref, 1);
  mat4_identity(matrix->m);
  return matrix;
}

void lovrMat4Destroy(void* ref) {
  lovrFree(ref);
}

float* lovrMat4GetData(Mat4* matrix) {
  return matrix->m;
}

// RandomGenerator (compatible with LÖVE's)

// Thomas Wang's 64-bit integer hashing function:
// https://web.archive.org/web/20110807030012/http://www.cris.com/%7ETtwang/tech/inthash.htm
static uint64_t wangHash64(uint64_t key) {
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}

// 64 bit Xorshift implementation taken from the end of Sec. 3 (page 4) in
// George Marsaglia, "Xorshift RNGs", Journal of Statistical Software, Vol.8 (Issue 14), 2003
// Use an 'Xorshift*' variant, as shown here: http://xorshift.di.unimi.it

RandomGenerator* lovrRandomGeneratorCreate(void) {
  RandomGenerator* generator = lovrCalloc(sizeof(RandomGenerator));
lovr_atomic_store(&generator->ref, 1);
  Seed seed = { .b32 = { .lo = 0xCBBF7A44, .hi = 0x0139408D } };
  lovrRandomGeneratorSetSeed(generator, seed);
  generator->lastRandomNormal = HUGE_VAL;
  return generator;
}

void lovrRandomGeneratorDestroy(void* ref) {
  lovrFree(ref);
}

Seed lovrRandomGeneratorGetSeed(RandomGenerator* generator) {
  return generator->seed;
}

void lovrRandomGeneratorSetSeed(RandomGenerator* generator, Seed seed) {
  generator->seed = seed;

  do {
    seed.b64 = wangHash64(seed.b64);
  } while (seed.b64 == 0);

  generator->state = seed;
}

void lovrRandomGeneratorGetState(RandomGenerator* generator, char* state, size_t length) {
  snprintf(state, length, "0x%" PRIx64, generator->state.b64);
}

int lovrRandomGeneratorSetState(RandomGenerator* generator, const char* state) {
  char* end = NULL;
  Seed newState;
  newState.b64 = strtoull(state, &end, 16);
  if (end != NULL && *end != 0) {
    return 1;
  } else {
    generator->state = newState;
    return 0;
  }
}

double lovrRandomGeneratorRandom(RandomGenerator* generator) {
  generator->state.b64 ^= (generator->state.b64 >> 12);
  generator->state.b64 ^= (generator->state.b64 << 25);
  generator->state.b64 ^= (generator->state.b64 >> 27);
  uint64_t r = generator->state.b64 * 2685821657736338717ULL;
  union { uint64_t i; double d; } u;
  u.i = ((0x3FFULL) << 52) | (r >> 12);
  return u.d - 1.;
}

double lovrRandomGeneratorRandomNormal(RandomGenerator* generator) {
  if (generator->lastRandomNormal != HUGE_VAL) {
    double r = generator->lastRandomNormal;
    generator->lastRandomNormal = HUGE_VAL;
    return r;
  }

  double a = lovrRandomGeneratorRandom(generator);
  double b = lovrRandomGeneratorRandom(generator);
  double r = sqrt(-2. * log(1. - a));
  double phi = 2. * M_PI * (1. - b);
  generator->lastRandomNormal = r * cos(phi);
  return r * sin(phi);
}
