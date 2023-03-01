#include "math.h"
#include "core/maf.h"
#include "util.h"
#include "lib/noise/simplexnoise1234.h"
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

struct Curve {
  uint32_t ref;
  arr_t(float) points;
};

struct LightProbe {
  uint32_t ref;
  float coefficients[9][3];
};

struct Pool {
  uint32_t ref;
  float* data;
  uint32_t count;
  uint32_t cursor;
  uint32_t generation;
};

struct RandomGenerator {
  uint32_t ref;
  Seed seed;
  Seed state;
  double lastRandomNormal;
};

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

RandomGenerator* lovrMathGetRandomGenerator() {
  return state.generator;
}

// Curve

// Explicit curve evaluation, unroll simple cases to avoid pow overhead
static void evaluate(float* restrict P, size_t n, float t, vec3 p) {
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
  Curve* curve = calloc(1, sizeof(Curve));
  lovrAssert(curve, "Out of memory");
  curve->ref = 1;
  arr_init(&curve->points, arr_alloc);
  arr_reserve(&curve->points, 16);
  return curve;
}

void lovrCurveDestroy(void* ref) {
  Curve* curve = ref;
  arr_free(&curve->points);
  free(curve);
}

void lovrCurveEvaluate(Curve* curve, float t, vec3 p) {
  lovrAssert(curve->points.length >= 8, "Need at least 2 points to evaluate a Curve");
  lovrAssert(t >= 0.f && t <= 1.f, "Curve evaluation interval must be within [0, 1]");
  evaluate(curve->points.data, curve->points.length / 4, t, p);
}

void lovrCurveGetTangent(Curve* curve, float t, vec3 p) {
  float q[4];
  size_t n = curve->points.length / 4;
  evaluate(curve->points.data, n - 1, t, q);
  evaluate(curve->points.data + 4, n - 1, t, p);
  vec3_add(p, vec3_scale(q, -1.f));
  vec3_normalize(p);
}

Curve* lovrCurveSlice(Curve* curve, float t1, float t2) {
  lovrAssert(curve->points.length >= 8, "Need at least 2 points to slice a Curve");
  lovrAssert(t1 >= 0.f && t2 <= 1.f, "Curve slice interval must be within [0, 1]");

  Curve* new = lovrCurveCreate();
  arr_reserve(&new->points, curve->points.length);
  new->points.length = curve->points.length;

  size_t n = curve->points.length / 4;

  // Right half of split at t1
  for (size_t i = 0; i < n - 1; i++) {
    evaluate(curve->points.data + 4 * i, n - i, t1, new->points.data + 4 * i);
  }

  vec3_init(new->points.data + 4 * (n - 1), curve->points.data + 4 * (n - 1));

  // Split segment at t2, taking left half
  float t = (t2 - t1) / (1.f - t1);
  for (size_t i = n - 1; i >= 1; i--) {
    evaluate(new->points.data, i + 1, t, new->points.data + 4 * i);
  }

  return new;
}

size_t lovrCurveGetPointCount(Curve* curve) {
  return curve->points.length / 4;
}

void lovrCurveGetPoint(Curve* curve, size_t index, vec3 point) {
  vec3_init(point, curve->points.data + 4 * index);
}

void lovrCurveSetPoint(Curve* curve, size_t index, vec3 point) {
  vec3_init(curve->points.data + 4 * index, point);
}

void lovrCurveAddPoint(Curve* curve, vec3 point, size_t index) {

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

// LightProbe

LightProbe* lovrLightProbeCreate(void) {
  LightProbe* probe = calloc(1, sizeof(LightProbe));
  lovrAssert(probe, "Out of memory");
  lovrLightProbeClear(probe);
  probe->ref = 1;
  return probe;
}

void lovrLightProbeDestroy(void* ref) {
  LightProbe* probe = ref;
  free(probe);
}

void lovrLightProbeClear(LightProbe* probe) {
  memset(probe->coefficients, 0, sizeof(probe->coefficients));
}

void lovrLightProbeGetCoefficients(LightProbe* probe, float coefficients[9][3]) {
  memcpy(coefficients, probe->coefficients, sizeof(probe->coefficients));
}

void lovrLightProbeSetCoefficients(LightProbe* probe, float coefficients[9][3]) {
  memcpy(probe->coefficients, coefficients, sizeof(probe->coefficients));
}

void lovrLightProbeEvaluate(LightProbe* probe, float normal[4], float color[4]) {
  memset(color, 0, 4 * sizeof(float));

  float n[4];
  vec3_init(n, normal);
  vec3_normalize(n);

  for (uint32_t i = 0; i < 3; i++) {
    color[i] += .88622692545276f * probe->coefficients[0][i];

    color[i] += 1.0233267079465f * probe->coefficients[1][i] * n[1];
    color[i] += 1.0233267079465f * probe->coefficients[2][i] * n[2];
    color[i] += 1.0233267079465f * probe->coefficients[3][i] * n[0];

    color[i] += .85808553080978f * probe->coefficients[4][i] * n[0] * n[1];
    color[i] += .85808553080978f * probe->coefficients[5][i] * n[1] * n[2];
    color[i] += .24770795610038f * probe->coefficients[6][i] * (3.f * n[2] * n[2] - 1.f);
    color[i] += .85808553080978f * probe->coefficients[7][i] * n[0] * n[2];
    color[i] += .42904276540489f * probe->coefficients[8][i] * (n[0] * n[0] - n[1] * n[1]);
  }
}

void lovrLightProbeAddAmbientLight(LightProbe* probe, float color[4]) {
  float scale = 3.544907701811f; // 2 * math.pi ^ .5
  probe->coefficients[0][0] += scale * .28209479177388f * color[0];
  probe->coefficients[0][1] += scale * .28209479177388f * color[1];
  probe->coefficients[0][2] += scale * .28209479177388f * color[2];
}

void lovrLightProbeAddDirectionalLight(LightProbe* probe, float direction[4], float color[4]) {
  float dir[4];
  vec3_init(dir, direction);
  vec3_normalize(dir);

  float scale = 2.9567930857316f; // 16 * math.pi / 17

  for (uint32_t i = 0; i < 3; i++) {
    probe->coefficients[0][i] += scale * .28209479177388f * color[i];
    probe->coefficients[1][i] += scale * .48860251190292f * color[i] * dir[1];
    probe->coefficients[2][i] += scale * .48860251190292f * color[i] * dir[2];
    probe->coefficients[3][i] += scale * .48860251190292f * color[i] * dir[0];
    probe->coefficients[4][i] += scale * 1.0925484305921f * color[i] * dir[0] * dir[1];
    probe->coefficients[5][i] += scale * 1.0925484305921f * color[i] * dir[1] * dir[2];
    probe->coefficients[6][i] += scale * .31539156525252f * color[i] * (3.f * dir[2] * dir[2] - 1.f);
    probe->coefficients[7][i] += scale * 1.0925484305921f * color[i] * dir[0] * dir[2];
    probe->coefficients[8][i] += scale * .54627421529604f * color[i] * (dir[0] * dir[0] - dir[1] * dir[1]);
  }
}

void lovrLightProbeAddCubemap(LightProbe* probe, uint32_t size, fn_pixel* getPixel, void* context) {
  float coefficients[9][3];
  memset(coefficients, 0, sizeof(coefficients));

  float totalAngle = 0.;
  for (uint32_t face = 0; face < 6; face++) {
    for (uint32_t y = 0; y < size; y++) {
      for (uint32_t x = 0; x < size; x++) {
        float u = 2.f * ((float) x + 0.5f) / (float) size - 1.f;
        float v = 2.f * ((float) y + 0.5f) / (float) size - 1.f;
        float dx, dy, dz;

        switch (face) {
          case 0: dx = -1.f; dy = -v; dz = -u; break;
          case 1: dx = 1.f; dy = -v; dz = u; break;
          case 2: dx = -u; dy = 1.f; dz = v; break;
          case 3: dx = -u; dy = -1.f; dz = -v; break;
          case 4: dx = -u; dy = -v; dz = 1.f; break;
          case 5: dx = u; dy = -v; dz = -1.f; break;
        }

        float dot = dx * dx + dy * dy + dz * dz;
        float length = sqrtf(dot);

        dx /= length;
        dy /= length;
        dz /= length;

        float solidAngle = 4.f / (dot * length);
        totalAngle += solidAngle;

        float color[4];
        getPixel(context, x, y, face, color);

        color[0] *= solidAngle;
        color[1] *= solidAngle;
        color[2] *= solidAngle;

        for (uint32_t c = 0; c < 3; c++) {
          coefficients[0][c] += .28209479177388f * color[c];
          coefficients[1][c] += .48860251190292f * color[c] * dy;
          coefficients[2][c] += .48860251190292f * color[c] * dz;
          coefficients[3][c] += .48860251190292f * color[c] * dx;
          coefficients[4][c] += 1.0925484305921f * color[c] * dx * dy;
          coefficients[5][c] += 1.0925484305921f * color[c] * dy * dz;
          coefficients[6][c] += .31539156525252f * color[c] * (3.f * dz * dz - 1.f);
          coefficients[7][c] += 1.0925484305921f * color[c] * dx * dz;
          coefficients[8][c] += .54627421529604f * color[c] * (dx * dx - dy * dy);
        }
      }
    }
  }

  for (uint32_t i = 0; i < 9; i++) {
    for (uint32_t c = 0; c < 3; c++) {
      probe->coefficients[i][c] += (coefficients[i][c] / totalAngle) * 4.f * M_PI;
    }
  }
}

void lovrLightProbeAddEquirect(LightProbe* probe, uint32_t width, uint32_t height, fn_pixel* getPixel, void* context) {
  float coefficients[9][3];
  memset(coefficients, 0, sizeof(coefficients));

  float totalAngle = 0.f;
  float pi = (float) M_PI;
  for (uint32_t y = 0; y < height; y++) {
    float phi = y / (float) height * pi;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t x = 0; x < width; x++) {
      float theta = (.75f - x / (float) width) * (2.f * pi);
      float solidAngle = (2.f * pi / width) * (pi / height) * fabsf(cosphi);
      totalAngle += solidAngle;

      float color[4];
      getPixel(context, x, y, 0, color);

      color[0] *= solidAngle;
      color[1] *= solidAngle;
      color[2] *= solidAngle;

      float dx = cosf(theta) * sinphi;
      float dy = cosphi;
      float dz = -sinf(theta) * sinphi;

      for (uint32_t c = 0; c < 3; c++) {
        coefficients[0][c] += .28209479177388f * color[c];
        coefficients[1][c] += .48860251190292f * color[c] * dy;
        coefficients[2][c] += .48860251190292f * color[c] * dz;
        coefficients[3][c] += .48860251190292f * color[c] * dx;
        coefficients[4][c] += 1.0925484305921f * color[c] * dx * dy;
        coefficients[5][c] += 1.0925484305921f * color[c] * dy * dz;
        coefficients[6][c] += .31539156525252f * color[c] * (3.f * dz * dz - 1.f);
        coefficients[7][c] += 1.0925484305921f * color[c] * dx * dz;
        coefficients[8][c] += .54627421529604f * color[c] * (dx * dx - dy * dy);
      }
    }
  }

  for (uint32_t i = 0; i < 9; i++) {
    for (uint32_t c = 0; c < 3; c++) {
      probe->coefficients[i][c] += (coefficients[i][c] / totalAngle) * 4.f * pi;
    }
  }
}

void lovrLightProbeAddProbe(LightProbe* probe, LightProbe* other) {
  float* a = &probe->coefficients[0][0];
  float* b = &other->coefficients[0][0];
  for (uint32_t i = 0; i < 27; i++) {
    a[i] += b[i];
  }
}

void lovrLightProbeLerp(LightProbe* probe, LightProbe* other, float t) {
  float* a = &probe->coefficients[0][0];
  float* b = &other->coefficients[0][0];
  for (uint32_t i = 0; i < 27; i++) {
    a[i] = a[i] + (b[i] - a[i]) * t;
  }
}

void lovrLightProbeScale(LightProbe* probe, float scale) {
  float* c = &probe->coefficients[0][0];
  for (uint32_t i = 0; i < 27; i++) {
    c[i] *= scale;
  }
}

// Pool

static const size_t vectorComponents[] = {
  [V_VEC2] = 2,
  [V_VEC3] = 4,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

Pool* lovrPoolCreate() {
  Pool* pool = calloc(1, sizeof(Pool));
  lovrAssert(pool, "Out of memory");
  pool->ref = 1;
  lovrPoolGrow(pool, 1 << 12);
  return pool;
}

void lovrPoolDestroy(void* ref) {
  Pool* pool = ref;
  free(pool->data);
  free(pool);
}

void lovrPoolGrow(Pool* pool, size_t count) {
  lovrAssert(count <= (1 << 24), "Temporary vector space exhausted.  Try using lovr.math.drain to drain the vector pool periodically.");
  pool->count = count;
  pool->data = realloc(pool->data, pool->count * sizeof(float));
  lovrAssert(pool->data, "Out of memory");
}

Vector lovrPoolAllocate(Pool* pool, VectorType type, float** data) {
  lovrCheck(pool, "The math module must be initialized to create vectors");

  size_t count = vectorComponents[type];

  if (pool->cursor + count > pool->count) {
    lovrPoolGrow(pool, pool->count * 2);
  }

  Vector v = {
    .handle = {
      .type = type,
      .generation = pool->generation,
      .index = pool->cursor
    }
  };

  *data = pool->data + pool->cursor;
  pool->cursor += count;
  return v;
}

float* lovrPoolResolve(Pool* pool, Vector vector) {
  lovrAssert(vector.handle.generation == pool->generation, "Attempt to use a temporary vector from a previous frame");
  return pool->data + vector.handle.index;
}

void lovrPoolDrain(Pool* pool) {
  pool->cursor = 0;
  pool->generation = (pool->generation + 1) & 0xf;
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
