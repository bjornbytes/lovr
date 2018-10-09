#include "math/curve.h"
#include <math.h>

// Explicit curve evaluation, unroll simple cases to avoid pow overhead
static void evaluate(float* P, int n, float t, vec3 p) {
  if (n == 2) {
    p[0] = P[0] + (P[3] - P[0]) * t;
    p[1] = P[1] + (P[4] - P[1]) * t;
    p[2] = P[2] + (P[5] - P[2]) * t;
  } else if (n == 3) {
    float t1 = (1 - t);
    float a = t1 * t1;
    float b = 2 * t1 * t;
    float c = t * t;
    p[0] = a * P[0] + b * P[3] + c * P[6];
    p[1] = a * P[1] + b * P[4] + c * P[7];
    p[2] = a * P[2] + b * P[5] + c * P[8];
  } else if (n == 4) {
    float t1 = (1 - t);
    float a = t1 * t1 * t1;
    float b = 3 * t1 * t1 * t;
    float c = 3 * t1 * t * t;
    float d = t * t * t;
    p[0] = a * P[0] + b * P[3] + c * P[6] + d * P[9];
    p[1] = a * P[1] + b * P[4] + c * P[7] + d * P[10];
    p[2] = a * P[2] + b * P[5] + c * P[8] + d * P[11];
  } else {
    float b = 1.f;
    p[0] = p[1] = p[2] = 0.f;
    for (int i = 0; i < n; i++, b *= (float) (n - i) / i) {
      float c1 = powf(1 - t, n - (i + 1));
      float c2 = powf(t, i);
      p[0] += b * c1 * c2 * P[i * 3 + 0];
      p[1] += b * c1 * c2 * P[i * 3 + 1];
      p[2] += b * c1 * c2 * P[i * 3 + 2];
    }
  }
}

Curve* lovrCurveCreate(int sizeHint) {
  Curve* curve = lovrAlloc(Curve, lovrCurveDestroy);
  if (!curve) return NULL;

  vec_init(&curve->points);
  lovrAssert(!vec_reserve(&curve->points, sizeHint * 3), "Out of memory");

  return curve;
}

void lovrCurveDestroy(void* ref) {
  Curve* curve = ref;
  vec_deinit(&curve->points);
  free(curve);
}

void lovrCurveEvaluate(Curve* curve, float t, vec3 p) {
  lovrAssert(curve->points.length >= 6, "Need at least 2 points to evaluate a Curve");
  lovrAssert(t >= 0 && t <= 1, "Curve evaluation interval must be within [0, 1]");
  evaluate(curve->points.data, curve->points.length / 3, t, p);
}

void lovrCurveRender(Curve* curve, float t1, float t2, vec3 points, int n) {
  lovrAssert(curve->points.length >= 6, "Need at least 2 points to render a Curve");
  lovrAssert(t1 >= 0 && t2 <= 1, "Curve render interval must be within [0, 1]");
  float step = 1.f / (n - 1);
  for (int i = 0; i < n; i++) {
    evaluate(curve->points.data, curve->points.length / 3, t1 + (t2 - t1) * i * step, points + 3 * i);
  }
}

Curve* lovrCurveSplit(Curve* curve, float t1, float t2) {
  lovrAssert(curve->points.length >= 6, "Need at least 2 points to split a Curve");
  lovrAssert(t1 >= 0 && t2 <= 1, "Curve split interval must be within [0, 1]");

  Curve* new = lovrCurveCreate(curve->points.length / 3);
  new->points.length = curve->points.length;

  int n = curve->points.length / 3;

  // Right half of split at t1
  for (int i = 0; i < n - 1; i++) {
    evaluate(curve->points.data + 3 * i, n - i, t1, new->points.data + 3 * i);
  }

  vec3_init(new->points.data + 3 * (n - 1), curve->points.data + 3 * (n - 1));

  // Split segment at t2, taking left half
  float t = (t2 - t1) / (1.f - t1);
  for (int i = n - 1; i >= 1; i--) {
    evaluate(new->points.data, i + 1, t, new->points.data + 3 * i);
  }

  return new;
}

int lovrCurveGetPointCount(Curve* curve) {
  return curve->points.length / 3;
}

void lovrCurveGetPoint(Curve* curve, int index, vec3 point) {
  vec3_init(point, curve->points.data + 3 * index);
}

void lovrCurveSetPoint(Curve* curve, int index, vec3 point) {
  vec3_init(curve->points.data + 3 * index, point);
}

void lovrCurveAddPoint(Curve* curve, vec3 point, int index) {

  // Reserve enough memory for 3 more points, then record destination once memory is allocated
  lovrAssert(!vec_reserve(&curve->points, curve->points.length + 3), "Out of memory");
  float* dest = curve->points.data + index * 3;

  // Shift remaining points over (if any) to create empty space
  if (index * 3 != curve->points.length) {
    memmove(dest + 3, dest, (curve->points.length - index * 3) * sizeof(float));
  }

  // Fill the empty space with the new point
  curve->points.length += 3;
  memcpy(dest, point, 3 * sizeof(float));
}

void lovrCurveRemovePoint(Curve* curve, int index) {
  vec_swapsplice(&curve->points, index * 3, 3);
}
