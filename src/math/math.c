#include "math.h"
#include "lib/noise1234/noise1234.h"
#include "util.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static MathState state;

bool lovrMathInit(size_t poolSize) {
  if (state.initialized) return false;
  state.pool = lovrPoolCreate(poolSize, true);
  state.generator = lovrRandomGeneratorCreate();
  Seed seed = { .b64 = (uint64_t) time(0) };
  lovrRandomGeneratorSetSeed(state.generator, seed);
  return state.initialized = true;
}

void lovrMathDestroy() {
  if (!state.initialized) return;
  lovrRelease(state.pool);
  lovrRelease(state.generator);
  memset(&state, 0, sizeof(MathState));
}

Pool* lovrMathGetPool() {
  return state.pool;
}

RandomGenerator* lovrMathGetRandomGenerator() {
  return state.generator;
}

void lovrMathOrientationToDirection(float angle, float ax, float ay, float az, vec3 v) {
  float sinTheta = sinf(angle);
  float cosTheta = cosf(angle);
  v[0] = sinTheta * -ay + (1.f - cosTheta) * -az * ax;
  v[1] = sinTheta * ax + (1.f - cosTheta) * -az * ay;
  v[2] = -cosTheta + (1.f - cosTheta) * -az * az;
}

float lovrMathGammaToLinear(float x) {
  if (x <= .04045) {
    return x / 12.92;
  } else {
    return powf((x + .055) / 1.055, 2.4);
  }
}

float lovrMathLinearToGamma(float x) {
  if (x <= .0031308) {
    return x * 12.92;
  } else {
    return 1.055 * powf(x, 1. / 2.4) - .055;
  }
}

float lovrMathNoise1(float x) {
  return noise1(x) * .5f + .5f;
}

float lovrMathNoise2(float x, float y) {
  return noise2(x, y) * .5f + .5f;
}

float lovrMathNoise3(float x, float y, float z) {
  return noise3(x, y, z) * .5f + .5f;
}

float lovrMathNoise4(float x, float y, float z, float w) {
  return noise4(x, y, z, w) * .5f + .5f;
}
