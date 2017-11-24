#include "math.h"
#include "math/vec3.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

static RandomGenerator* generator;

void lovrMathInit() {
  generator = lovrRandomGeneratorCreate();
  Seed seed = { .b64 = (uint64_t) time(0) };
	lovrRandomGeneratorSetSeed(generator, seed);
  atexit(lovrMathDestroy);
}

void lovrMathDestroy() {
  lovrRandomGeneratorDestroy(&generator->ref);
}

RandomGenerator* lovrMathGetRandomGenerator() {
  return generator;
}

void lovrMathOrientationToDirection(float angle, float ax, float ay, float az, vec3 v) {
  float sinTheta = sin(angle);
  float cosTheta = cos(angle);
  v[0] = sinTheta * -ay + (1 - cosTheta) * -az * ax;
  v[1] = sinTheta * ax + (1 - cosTheta) * -az * ay;
  v[2] = -cosTheta + (1 - cosTheta) * -az * az;
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
