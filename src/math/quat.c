#include "math/quat.h"
#include "math/vec3.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>

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

quat quat_fromDirection(quat q, vec3 forward, vec3 up) {
  vec3 qq = (vec3) q;
  vec3_init(qq, forward);
  vec3_normalize(qq);
  q[3] = 1 + vec3_dot(qq, up);
  vec3_cross(qq, up);
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

quat quat_multiply(quat q, quat r) {
  float qx = q[0];
  float qy = q[1];
  float qz = q[2];
  float qw = q[3];
  float rx = r[0];
  float ry = r[1];
  float rz = r[2];
  float rw = r[3];
  q[0] = qx * rw + qw * rx - qy * rz - qz * ry;
  q[1] = qy * rw + qw * ry - qz * rx - qx * rz;
  q[2] = qz * rw + qw * rz - qx * ry - qy * rx;
  q[3] = qw * rw - qx * rx - qy * ry - qz * rz;
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

// From gl-matrix
quat quat_slerp(quat q, quat r, float t) {
  float dot = q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3];
  if (fabs(dot) >= 1.f) {
    return q;
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

quat quat_between(quat q, vec3 u, vec3 v) {
  float dot = vec3_dot(u, v);
  if (dot > .99999) {
    q[0] = q[1] = q[2] = 0.f;
    q[3] = 1.f;
    return q;
  } else if (dot < -.99999) {
    float axis[3];
    vec3_cross(vec3_set(axis, 1, 0, 0), u);
    if (vec3_length(axis) < .00001) {
      vec3_cross(vec3_set(axis, 0, 1, 0), u);
    }
    vec3_normalize(axis);
    quat_fromAngleAxis(q, M_PI, axis);
    return q;
  }

  vec3_cross(vec3_init(q, u), v);
  q[3] = 1 + dot;
  return quat_normalize(q);
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
