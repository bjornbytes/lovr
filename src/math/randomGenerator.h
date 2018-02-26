#include <stdint.h>
#include "util.h"

#pragma once

// Direct port of LÖVE's RandomGenerator

typedef union {
  uint64_t b64;
  struct {
    uint32_t lo;
    uint32_t hi;
  } b32;
} Seed;

typedef struct {
  Ref ref;
  Seed seed;
  Seed state;
  double lastRandomNormal;
} RandomGenerator;

RandomGenerator* lovrRandomGeneratorCreate();
Seed lovrRandomGeneratorGetSeed(RandomGenerator* generator);
void lovrRandomGeneratorSetSeed(RandomGenerator* generator, Seed seed);
void lovrRandomGeneratorGetState(RandomGenerator* generator, char* state, size_t length);
int lovrRandomGeneratorSetState(RandomGenerator* generator, const char* state, size_t length);
double lovrRandomGeneratorRandom(RandomGenerator* generator);
double lovrRandomGeneratorRandomNormal(RandomGenerator* generator);
