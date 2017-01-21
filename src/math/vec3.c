#include "math/vec3.h"
#include <math.h>
#include <stdlib.h>

vec3 vec3_init(vec3 v, vec3 u) {
  return vec3_set(v, u[0], u[1], u[2]);
}

vec3 vec3_set(vec3 v, float x, float y, float z) {
  v[0] = x;
  v[1] = y;
  v[2] = z;
  return v;
}

vec3 vec3_add(vec3 v, vec3 u) {
  v[0] += u[0];
  v[1] += u[1];
  v[2] += u[2];
  return v;
}

vec3 vec3_scale(vec3 v, float s) {
  v[0] *= s;
  v[1] *= s;
  v[2] *= s;
  return v;
}

vec3 vec3_normalize(vec3 v) {
  float len = vec3_length(v);
  return len == 0 ? v : vec3_scale(v, 1 / len);
}

float vec3_length(vec3 v) {
  return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

float vec3_dot(vec3 v, vec3 u) {
  return v[0] * u[0] + v[1] * u[1] + v[2] * u[2];
}

vec3 vec3_cross(vec3 v, vec3 u) {
  return vec3_set(v,
    v[1] * u[2] - v[2] * u[1],
    v[2] * u[0] - v[0] * u[2],
    v[0] * u[1] - v[1] * u[0]
  );
}
