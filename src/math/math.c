#include "math.h"
#include "lib/noise1234/noise1234.h"
#include "util.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static MathState state;

bool lovrMathInit() {
  if (state.initialized) return false;
  state.generator = lovrRandomGeneratorCreate();
  Seed seed = { .b64 = (uint64_t) time(0) };
  lovrRandomGeneratorSetSeed(state.generator, seed);
  return state.initialized = true;
}

void lovrMathDestroy() {
  if (!state.initialized) return;
  lovrRelease(RandomGenerator, state.generator);
  memset(&state, 0, sizeof(MathState));
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
