#include "math.h"
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
