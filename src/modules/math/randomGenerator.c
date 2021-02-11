#include "math/randomGenerator.h"
#include "core/util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

struct RandomGenerator {
  uint32_t ref;
  Seed seed;
  Seed state;
  double lastRandomNormal;
};

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
  RandomGenerator* generator = calloc(1, sizeof(RandomGenerator));
  lovrAssert(generator, "Out of memory");
  generator->ref = 1;
  Seed seed = { .b32 = { .lo = 0xCBBF7A44, .hi = 0x0139408D } };
  lovrRandomGeneratorSetSeed(generator, seed);
  generator->lastRandomNormal = HUGE_VAL;
  return generator;
}

void lovrRandomGeneratorDestroy(void* ref) {
  free(ref);
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
