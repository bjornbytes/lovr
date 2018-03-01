#include "math/quat.h"
#include "math/vec3.h"
#include <math.h>

quat quat_init(quat q, quat r) {
  return quat_set(q, r[0], r[1], r[2], r[3]);
}

quat quat_set(quat q, float x, float y, float z, float w) {
  q[0] = x;
  q[1] = y;
  q[2] = z;
  q[3] = w;
  return q;
}

quat quat_fromAngleAxis(quat q, float angle, vec3 axis) {
  vec3_normalize(axis);
  float s = sin(angle * .5f);
  float c = cos(angle * .5f);
  q[0] = s * axis[0];
  q[1] = s * axis[1];
  q[2] = s * axis[2];
  q[3] = c;
  return q;
}

quat quat_fromMat4(quat q, mat4 m) {
  float x = sqrt(MAX(0, 1 + m[0] - m[5] - m[10])) / 2;
  float y = sqrt(MAX(0, 1 - m[0] + m[5] - m[10])) / 2;
  float z = sqrt(MAX(0, 1 - m[0] - m[5] + m[10])) / 2;
  float w = sqrt(MAX(0, 1 + m[0] + m[5] + m[10])) / 2;
  x = (m[9] - m[6]) > 0 ? -x : x;
  y = (m[2] - m[8]) > 0 ? -y : y;
  z = (m[4] - m[1]) > 0 ? -z : z;
  q[0] = x;
  q[1] = y;
  q[2] = z;
  q[3] = w;
  return q;
}

quat quat_normalize(quat q) {
  float len = quat_length(q);
  if (len == 0) {
    return q;
  }

  len = 1 / len;
  q[0] *= len;
  q[1] *= len;
  q[2] *= len;
  q[3] *= len;
  return q;
}

float quat_length(quat q) {
  return sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

quat quat_slerp(quat q, quat r, float t) {
  float dot = q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3];
  if (fabs(dot) >= 1.f) {
    return q;
  }

  if (dot < 0) {
    q[0] *= -1;
    q[1] *= -1;
    q[2] *= -1;
    q[3] *= -1;
    dot *= -1;
  }

  float halfTheta = acos(dot);
  float sinHalfTheta = sqrt(1.f - dot * dot);

  if (fabs(sinHalfTheta) < .001) {
    q[0] = q[0] * .5 + r[0] * .5;
    q[1] = q[1] * .5 + r[1] * .5;
    q[2] = q[2] * .5 + r[2] * .5;
    q[3] = q[3] * .5 + r[3] * .5;
    return q;
  }

  float a = sin((1 - t) * halfTheta) / sinHalfTheta;
  float b = sin(t * halfTheta) / sinHalfTheta;

  q[0] = q[0] * a + r[0] * b;
  q[1] = q[1] * a + r[1] * b;
  q[2] = q[2] * a + r[2] * b;
  q[3] = q[3] * a + r[3] * b;

  return q;
}

void quat_rotate(quat q, vec3 v) {
  float s = q[3];
  float u[3];
  float c[3];
  vec3_init(u, q);
  vec3_cross(vec3_init(c, u), v);
  float uu = vec3_dot(u, u);
  float uv = vec3_dot(u, v);
  vec3_scale(u, 2 * uv);
  vec3_scale(v, s * s - uu);
  vec3_scale(c, 2 * s);
  vec3_add(v, vec3_add(u, c));
}

void quat_getAngleAxis(quat q, float* angle, float* x, float* y, float* z) {
  if (q[3] > 1 || q[3] < -1) {
    quat_normalize(q);
  }

  float qw = q[3];
  float s = sqrt(1 - qw * qw);
  s = s < .0001 ? 1 : 1 / s;
  *angle = 2 * acos(qw);
  *x = q[0] * s;
  *y = q[1] * s;
  *z = q[2] * s;
}
