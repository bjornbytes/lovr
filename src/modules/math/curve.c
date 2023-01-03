#include "math/curve.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <math.h>

struct Curve {
  uint32_t ref;
  arr_t(float) points;
};

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
