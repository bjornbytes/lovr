#include "math/vec3.h"
#include <math.h>
#include <stdlib.h>

vec3 vec3_init(float x, float y, float z) {
  vec3 v = malloc(3 * sizeof(float));
  v[0] = x;
  v[1] = y;
  v[2] = z;
  return v;
}

vec3 vec3_set(vec3 v, vec3 u) {
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  return v;
}

vec3 vec3_add(vec3 v, vec3 u) {
  v[0] += u[0];
  v[1] += u[1];
  v[2] += u[2];
  return v;
}

vec3 vec3_sub(vec3 v, vec3 u) {
  v[0] -= u[0];
  v[1] -= u[1];
  v[2] -= u[2];
  return v;
}

vec3 vec3_mul(vec3 v, vec3 u) {
  v[0] *= u[0];
  v[1] *= u[1];
  v[2] *= u[2];
  return v;
}

vec3 vec3_div(vec3 v, vec3 u) {
  v[0] /= u[0];
  v[1] /= u[1];
  v[2] /= u[2];
  return v;
}

vec3 vec3_normalize(vec3 v) {
  float len = vec3_len(v);
  if (len == 0) {
    return v;
  }

  len = 1 / len;
  v[0] *= len;
  v[1] *= len;
  v[2] *= len;
  return v;
}

float vec3_len(vec3 v) {
  return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

float vec3_dist(vec3 v, vec3 u) {
  float dx = v[0] - u[0];
  float dy = v[1] - u[1];
  float dz = v[2] - u[2];
  return sqrt(dx * dx + dy * dy + dz * dz);
}

float vec3_angle(vec3 v, vec3 u) {
  return acos(vec3_dot(v, u) / (vec3_len(v) * vec3_len(u)));
}

float vec3_dot(vec3 v, vec3 u) {
  return v[0] * u[0] + v[1] * u[1] + v[2] * u[2];
}

vec3 vec3_cross(vec3 v, vec3 u) {
  float v0 = v[0];
  float v1 = v[1];
  float v2 = v[2];
  v[0] =  v1 * u[2] - v2 * u[1];
  v[1] =  v0 * u[2] - v2 * u[0];
  v[2] =  v0 * u[1] - v1 * u[0];
  return v;
}

vec3 vec3_lerp(vec3 v, vec3 u, float t) {
  v[0] += (u[0] - v[0]) * t;
  v[1] += (u[1] - v[1]) * t;
  v[2] += (u[2] - v[2]) * t;
  return v;
}
