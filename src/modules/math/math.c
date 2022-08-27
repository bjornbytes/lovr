#include "math.h"
#include "math/randomGenerator.h"
#include "util.h"
#include "lib/noise/simplexnoise1234.h"
#include <math.h>
#include <string.h>
#include <time.h>

static struct {
  bool initialized;
  RandomGenerator* generator;
} state;

bool lovrMathInit() {
  if (state.initialized) return false;
  state.generator = lovrRandomGeneratorCreate();
  Seed seed = { .b64 = (uint64_t) time(0) };
  lovrRandomGeneratorSetSeed(state.generator, seed);
  return state.initialized = true;
}

void lovrMathDestroy() {
  if (!state.initialized) return;
  lovrRelease(state.generator, lovrRandomGeneratorDestroy);
  memset(&state, 0, sizeof(state));
}

RandomGenerator* lovrMathGetRandomGenerator() {
  return state.generator;
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
