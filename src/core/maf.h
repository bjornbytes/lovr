#include <string.h>
#include <math.h>
#include "util.h"

#pragma once

#ifdef MAF_EXPORT
#define MAF LOVR_EXPORT
#else
#define MAF static LOVR_INLINE
#endif

typedef float* vec3;
typedef float* quat;
typedef float* mat4;

// vec3

MAF vec3 vec3_set(vec3 v, float x, float y, float z) {
  v[0] = x;
  v[1] = y;
  v[2] = z;
  return v;
}

MAF vec3 vec3_init(vec3 v, const vec3 u) {
  float x = u[0], y = u[1], z = u[2], w = u[3];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF vec3 vec3_add(vec3 v, const vec3 u) {
  float x = v[0] + u[0], y = v[1] + u[1], z = v[2] + u[2], w = v[3] + u[3];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF vec3 vec3_sub(vec3 v, const vec3 u) {
  float x = v[0] - u[0], y = v[1] - u[1], z = v[2] - u[2], w = v[3] - u[3];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF vec3 vec3_scale(vec3 v, float s) {
  v[0] *= s;
  v[1] *= s;
  v[2] *= s;
  v[3] *= s;
  return v;
}

MAF float vec3_length(const vec3 v) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

MAF vec3 vec3_normalize(vec3 v) {
  float len = vec3_length(v);
  return len == 0.f ? v : vec3_scale(v, 1.f / len);
}

MAF float vec3_distance(const vec3 v, const vec3 u) {
  float dx = v[0] - u[0];
  float dy = v[1] - u[1];
  float dz = v[2] - u[2];
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

MAF float vec3_dot(const vec3 v, const vec3 u) {
  return v[0] * u[0] + v[1] * u[1] + v[2] * u[2];
}

MAF vec3 vec3_cross(vec3 v, const vec3 u) {
  return vec3_set(v,
    v[1] * u[2] - v[2] * u[1],
    v[2] * u[0] - v[0] * u[2],
    v[0] * u[1] - v[1] * u[0]
  );
}

MAF vec3 vec3_lerp(vec3 v, const vec3 u, float t) {
  float x = v[0] + (u[0] - v[0]) * t;
  float y = v[1] + (u[1] - v[1]) * t;
  float z = v[2] + (u[2] - v[2]) * t;
  float w = v[3] + (u[3] - v[3]) * t;
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF vec3 vec3_min(vec3 v, const vec3 u) {
  float x = MIN(v[0], u[0]);
  float y = MIN(v[1], u[1]);
  float z = MIN(v[2], u[2]);
  float w = MIN(v[3], u[3]);
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF vec3 vec3_max(vec3 v, const vec3 u) {
  float x = MAX(v[0], u[0]);
  float y = MAX(v[1], u[1]);
  float z = MAX(v[2], u[2]);
  float w = MAX(v[3], u[3]);
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

// quat

MAF quat quat_set(quat q, float x, float y, float z, float w) {
  q[0] = x;
  q[1] = y;
  q[2] = z;
  q[3] = w;
  return q;
}

MAF quat quat_init(quat q, const quat r) {
  return quat_set(q, r[0], r[1], r[2], r[3]);
}

MAF quat quat_fromAngleAxis(quat q, float angle, float ax, float ay, float az) {
  float s = sinf(angle * .5f);
  float c = cosf(angle * .5f);
  float length = sqrtf(ax * ax + ay * ay + az * az);
  if (length > 0.f) {
    s /= length;
  }
  return quat_set(q, s * ax, s * ay, s * az, c);
}

MAF quat quat_fromMat4(quat q, mat4 m) {
  float x = sqrtf(MAX(0.f, 1.f + m[0] - m[5] - m[10])) / 2.f;
  float y = sqrtf(MAX(0.f, 1.f - m[0] + m[5] - m[10])) / 2.f;
  float z = sqrtf(MAX(0.f, 1.f - m[0] - m[5] + m[10])) / 2.f;
  float w = sqrtf(MAX(0.f, 1.f + m[0] + m[5] + m[10])) / 2.f;
  x = (m[9] - m[6]) > 0.f ? -x : x;
  y = (m[2] - m[8]) > 0.f ? -y : y;
  z = (m[4] - m[1]) > 0.f ? -z : z;
  return quat_set(q, x, y, z, w);
}

MAF quat quat_mul(quat q, quat r) {
  return quat_set(q,
    q[0] * r[3] + q[3] * r[0] + q[1] * r[2] - q[2] * r[1],
    q[1] * r[3] + q[3] * r[1] + q[2] * r[0] - q[0] * r[2],
    q[2] * r[3] + q[3] * r[2] + q[0] * r[1] - q[1] * r[0],
    q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2]
  );
}

MAF float quat_length(quat q) {
  return sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

MAF quat quat_normalize(quat q) {
  float length = quat_length(q);
  if (length > 0.f) {
    q[0] /= length;
    q[1] /= length;
    q[2] /= length;
    q[3] /= length;
  }
  return q;
}

MAF void quat_getDirection(quat q, vec3 v) {
  v[0] = -2.f * q[0] * q[2] - 2.f * q[3] * q[1];
  v[1] = -2.f * q[1] * q[2] + 2.f * q[3] * q[0];
  v[2] = -1.f + 2.f * q[0] * q[0] + 2.f * q[1] * q[1];
}

MAF quat quat_conjugate(quat q) {
  q[0] = -q[0];
  q[1] = -q[1];
  q[2] = -q[2];
  return q;
}

MAF quat quat_slerp(quat q, quat r, float t) {
  float dot = q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3];
  if (fabsf(dot) >= 1.f) {
    return q;
  }

  if (dot < 0.f) {
    q[0] *= -1.f;
    q[1] *= -1.f;
    q[2] *= -1.f;
    q[3] *= -1.f;
    dot *= -1.f;
  }

  float halfTheta = acosf(dot);
  float sinHalfTheta = sqrtf(1.f - dot * dot);

  if (fabsf(sinHalfTheta) < .001f) {
    q[0] = q[0] * .5f + r[0] * .5f;
    q[1] = q[1] * .5f + r[1] * .5f;
    q[2] = q[2] * .5f + r[2] * .5f;
    q[3] = q[3] * .5f + r[3] * .5f;
    return q;
  }

  float a = sinf((1.f - t) * halfTheta) / sinHalfTheta;
  float b = sinf(t * halfTheta) / sinHalfTheta;

  q[0] = q[0] * a + r[0] * b;
  q[1] = q[1] * a + r[1] * b;
  q[2] = q[2] * a + r[2] * b;
  q[3] = q[3] * a + r[3] * b;

  return q;
}

MAF void quat_rotate(quat q, vec3 v) {
  float s = q[3];
  float u[4];
  float c[4];
  vec3_init(u, q);
  vec3_cross(vec3_init(c, u), v);
  float uu = vec3_dot(u, u);
  float uv = vec3_dot(u, v);
  vec3_scale(u, 2.f * uv);
  vec3_scale(v, s * s - uu);
  vec3_scale(c, 2.f * s);
  vec3_add(v, vec3_add(u, c));
}

MAF void quat_getAngleAxis(quat q, float* angle, float* x, float* y, float* z) {
  if (q[3] > 1.f || q[3] < -1.f) {
    quat_normalize(q);
  }

  float qw = q[3];
  float s = sqrtf(1.f - qw * qw);
  s = s < .0001f ? 1.f : 1.f / s;
  *angle = 2.f * acosf(qw);
  *x = q[0] * s;
  *y = q[1] * s;
  *z = q[2] * s;
}

MAF quat quat_between(quat q, vec3 u, vec3 v) {
  float dot = vec3_dot(u, v);
  if (dot > .99999f) {
    q[0] = q[1] = q[2] = 0.f;
    q[3] = 1.f;
    return q;
  } else if (dot < -.99999f) {
    float axis[4];
    vec3_cross(vec3_set(axis, 1.f, 0.f, 0.f), u);
    if (vec3_length(axis) < .00001f) {
      vec3_cross(vec3_set(axis, 0.f, 1.f, 0.f), u);
    }
    vec3_normalize(axis);
    quat_fromAngleAxis(q, (float) M_PI, axis[0], axis[1], axis[2]);
    return q;
  }
  vec3_cross(vec3_init(q, u), v);
  q[3] = 1.f + dot;
  return quat_normalize(q);
}

// mat4

#define MAT4_IDENTITY { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }

#define mat4_init mat4_set
MAF mat4 mat4_set(mat4 m, mat4 n) {
  quat_init(m + 0, n + 0);
  quat_init(m + 4, n + 4);
  quat_init(m + 8, n + 8);
  quat_init(m + 12, n + 12);
  return m;
}

MAF mat4 mat4_fromMat34(mat4 m, float (*n)[4]) {
  m[0] = n[0][0];
  m[1] = n[1][0];
  m[2] = n[2][0];
  m[3] = 0.f;
  m[4] = n[0][1];
  m[5] = n[1][1];
  m[6] = n[2][1];
  m[7] = 0.f;
  m[8] = n[0][2];
  m[9] = n[1][2];
  m[10] = n[2][2];
  m[11] = 0.f;
  m[12] = n[0][3];
  m[13] = n[1][3];
  m[14] = n[2][3];
  m[15] = 1.f;
  return m;
}

MAF mat4 mat4_fromMat44(mat4 m, float (*n)[4]) {
  m[0] = n[0][0];
  m[1] = n[1][0];
  m[2] = n[2][0];
  m[3] = n[3][0];
  m[4] = n[0][1];
  m[5] = n[1][1];
  m[6] = n[2][1];
  m[7] = n[3][1];
  m[8] = n[0][2];
  m[9] = n[1][2];
  m[10] = n[2][2];
  m[11] = n[3][2];
  m[12] = n[0][3];
  m[13] = n[1][3];
  m[14] = n[2][3];
  m[15] = n[3][3];
  return m;
}

MAF mat4 mat4_identity(mat4 m) {
  m[0] = 1.f;
  m[1] = 0.f;
  m[2] = 0.f;
  m[3] = 0.f;
  m[4] = 0.f;
  m[5] = 1.f;
  m[6] = 0.f;
  m[7] = 0.f;
  m[8] = 0.f;
  m[9] = 0.f;
  m[10] = 1.f;
  m[11] = 0.f;
  m[12] = 0.f;
  m[13] = 0.f;
  m[14] = 0.f;
  m[15] = 1.f;
  return m;
}

MAF mat4 mat4_transpose(mat4 m) {
  float a01 = m[1], a02 = m[2], a03 = m[3],
        a12 = m[6], a13 = m[7],
        a23 = m[11];

  m[1] = m[4];
  m[2] = m[8];
  m[3] = m[12];
  m[4] = a01;
  m[6] = m[9];
  m[7] = m[13];
  m[8] = a02;
  m[9] = a12;
  m[11] = m[14];
  m[12] = a03;
  m[13] = a13;
  m[14] = a23;
  return m;
}

MAF mat4 mat4_invert(mat4 m) {
  float a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3],
        a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7],
        a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11],
        a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15],

        b00 = a00 * a11 - a01 * a10,
        b01 = a00 * a12 - a02 * a10,
        b02 = a00 * a13 - a03 * a10,
        b03 = a01 * a12 - a02 * a11,
        b04 = a01 * a13 - a03 * a11,
        b05 = a02 * a13 - a03 * a12,
        b06 = a20 * a31 - a21 * a30,
        b07 = a20 * a32 - a22 * a30,
        b08 = a20 * a33 - a23 * a30,
        b09 = a21 * a32 - a22 * a31,
        b10 = a21 * a33 - a23 * a31,
        b11 = a22 * a33 - a23 * a32,

        d = (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06),
        invDet;

  if (!d) { return m; }
  invDet = 1 / d;

  m[0] = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
  m[1] = (-a01 * b11 + a02 * b10 - a03 * b09) * invDet;
  m[2] = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
  m[3] = (-a21 * b05 + a22 * b04 - a23 * b03) * invDet;
  m[4] = (-a10 * b11 + a12 * b08 - a13 * b07) * invDet;
  m[5] = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
  m[6] = (-a30 * b05 + a32 * b02 - a33 * b01) * invDet;
  m[7] = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;
  m[8] = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
  m[9] = (-a00 * b10 + a01 * b08 - a03 * b06) * invDet;
  m[10] = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
  m[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * invDet;
  m[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * invDet;
  m[13] = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
  m[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * invDet;
  m[15] = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;

  return m;
}

// Calculate matrix equivalent to "apply n, then m"
MAF mat4 mat4_multiply(mat4 m, mat4 n) {
  float m00 = m[0], m01 = m[1], m02 = m[2], m03 = m[3],
        m10 = m[4], m11 = m[5], m12 = m[6], m13 = m[7],
        m20 = m[8], m21 = m[9], m22 = m[10], m23 = m[11],
        m30 = m[12], m31 = m[13], m32 = m[14], m33 = m[15],

        n00 = n[0], n01 = n[1], n02 = n[2], n03 = n[3],
        n10 = n[4], n11 = n[5], n12 = n[6], n13 = n[7],
        n20 = n[8], n21 = n[9], n22 = n[10], n23 = n[11],
        n30 = n[12], n31 = n[13], n32 = n[14], n33 = n[15];

  m[0] = n00 * m00 + n01 * m10 + n02 * m20 + n03 * m30;
  m[1] = n00 * m01 + n01 * m11 + n02 * m21 + n03 * m31;
  m[2] = n00 * m02 + n01 * m12 + n02 * m22 + n03 * m32;
  m[3] = n00 * m03 + n01 * m13 + n02 * m23 + n03 * m33;
  m[4] = n10 * m00 + n11 * m10 + n12 * m20 + n13 * m30;
  m[5] = n10 * m01 + n11 * m11 + n12 * m21 + n13 * m31;
  m[6] = n10 * m02 + n11 * m12 + n12 * m22 + n13 * m32;
  m[7] = n10 * m03 + n11 * m13 + n12 * m23 + n13 * m33;
  m[8] = n20 * m00 + n21 * m10 + n22 * m20 + n23 * m30;
  m[9] = n20 * m01 + n21 * m11 + n22 * m21 + n23 * m31;
  m[10] = n20 * m02 + n21 * m12 + n22 * m22 + n23 * m32;
  m[11] = n20 * m03 + n21 * m13 + n22 * m23 + n23 * m33;
  m[12] = n30 * m00 + n31 * m10 + n32 * m20 + n33 * m30;
  m[13] = n30 * m01 + n31 * m11 + n32 * m21 + n33 * m31;
  m[14] = n30 * m02 + n31 * m12 + n32 * m22 + n33 * m32;
  m[15] = n30 * m03 + n31 * m13 + n32 * m23 + n33 * m33;
  return m;
}

MAF float* mat4_multiplyVec4(mat4 m, float* v) {
  float x = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
  float y = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
  float z = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
  float w = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  return v;
}

MAF mat4 mat4_translate(mat4 m, float x, float y, float z) {
  m[12] = m[0] * x + m[4] * y + m[8] * z + m[12];
  m[13] = m[1] * x + m[5] * y + m[9] * z + m[13];
  m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
  m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
  return m;
}

MAF mat4 mat4_rotateQuat(mat4 m, quat q) {
  float x = q[0], y = q[1], z = q[2], w = q[3];
  float m2[16] = {
    1 - 2 * y * y - 2 * z * z, 2 * x * y + 2 * w * z, 2 * x * z - 2 * w * y, 0,
    2 * x * y - 2 * w * z, 1 - 2 * x * x - 2 * z * z, 2 * y * z + 2 * w * x, 0,
    2 * x * z + 2 * w * y, 2 * y * z - 2 * w * x, 1 - 2 * x * x - 2 * y * y, 0,
    0, 0, 0, 1
  };
  return mat4_multiply(m, m2);
}

MAF mat4 mat4_rotate(mat4 m, float angle, float x, float y, float z) {
  float q[4];
  quat_fromAngleAxis(q, angle, x, y, z);
  return mat4_rotateQuat(m, q);
}

MAF mat4 mat4_scale(mat4 m, float x, float y, float z) {
  m[0] *= x;
  m[1] *= x;
  m[2] *= x;
  m[3] *= x;
  m[4] *= y;
  m[5] *= y;
  m[6] *= y;
  m[7] *= y;
  m[8] *= z;
  m[9] *= z;
  m[10] *= z;
  m[11] *= z;
  return m;
}

MAF void mat4_getPosition(mat4 m, vec3 position) {
  vec3_init(position, m + 12);
}

MAF void mat4_getOrientation(mat4 m, quat orientation) {
  quat_fromMat4(orientation, m);
}

MAF void mat4_getAngleAxis(mat4 m, float* angle, float* ax, float* ay, float* az) {
  float sx = vec3_length(m + 0);
  float sy = vec3_length(m + 4);
  float sz = vec3_length(m + 8);
  float diagonal[4] = { m[0], m[5], m[10] };
  float axis[4] = { m[6] - m[9], m[8] - m[2], m[1] - m[4] };
  diagonal[0] /= sx;
  diagonal[1] /= sy;
  diagonal[2] /= sz;
  vec3_normalize(axis);
  *angle = acosf((diagonal[0] + diagonal[1] + diagonal[2] - 1.f) / 2.f);
  *ax = axis[0];
  *ay = axis[1];
  *az = axis[2];
}

MAF void mat4_getScale(mat4 m, vec3 scale) {
  vec3_set(scale, vec3_length(m + 0), vec3_length(m + 4), vec3_length(m + 8));
}

MAF mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float clipNear, float clipFar) {
  float rl = right - left;
  float tb = top - bottom;
  float fn = clipFar - clipNear;
  memset(m, 0, 16 * sizeof(float));
  m[0] = 2 / rl;
  m[5] = 2 / tb;
  m[10] = -2 / fn;
  m[12] = -(left + right) / rl;
  m[13] = -(top + bottom) / tb;
  m[14] = -(clipFar + clipNear) / fn;
  m[15] = 1;
  return m;
}

MAF mat4 mat4_perspective(mat4 m, float clipNear, float clipFar, float fovy, float aspect) {
  float range = tanf(fovy * .5f) * clipNear;
  float sx = (2.f * clipNear) / (range * aspect + range * aspect);
  float sy = clipNear / range;
  float sz = -(clipFar + clipNear) / (clipFar - clipNear);
  float pz = (-2.f * clipFar * clipNear) / (clipFar - clipNear);
  memset(m, 0, 16 * sizeof(float));
  m[0] = sx;
  m[5] = sy;
  m[10] = sz;
  m[11] = -1.f;
  m[14] = pz;
  m[15] = 0.f;
  return m;
}

// This is currently specific to OpenGL
MAF mat4 mat4_fov(mat4 m, float left, float right, float up, float down, float clipNear, float clipFar) {
  float idx = 1.f / (right - left);
  float idy = 1.f / (up - down);
  float idz = 1.f / (clipFar - clipNear);
  float sx = right + left;
  float sy = down + up;
  memset(m, 0, 16 * sizeof(float));
  m[0] = 2.f * idx;
  m[5] = 2.f * idy;
  m[8] = sx * idx;
  m[9] = sy * idy;
  m[10] = -(clipFar + clipNear) * idz;
  m[11] = -1.f;
  m[14] = -(clipFar * 2.f * clipNear) * idz;
  return m;
}

MAF mat4 mat4_lookAt(mat4 m, vec3 from, vec3 to, vec3 up) {
  float x[4];
  float y[4];
  float z[4];
  vec3_normalize(vec3_sub(vec3_init(z, from), to));
  vec3_normalize(vec3_cross(vec3_init(x, up), z));
  vec3_cross(vec3_init(y, z), x);
  m[0] = x[0];
  m[1] = x[1];
  m[2] = x[2];
  m[3] = 0.f;
  m[4] = y[0];
  m[5] = y[1];
  m[6] = y[2];
  m[7] = 0.f;
  m[8] = z[0];
  m[9] = z[1];
  m[10] = z[2];
  m[11] = 0.f;
  m[12] = from[0];
  m[13] = from[1];
  m[14] = from[2];
  m[15] = 1.f;
  return m;
}

// Apply matrix to a vec3
// Difference from mat4_multiplyVec4: w normalize is performed, w in vec3 is ignored
MAF void mat4_transform(mat4 m, vec3 v) {
  float x = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12];
  float y = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13];
  float z = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
  float w = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + m[15];
  v[0] = x / w;
  v[1] = y / w;
  v[2] = z / w;
  v[3] = w / w;
}

MAF void mat4_transformDirection(mat4 m, vec3 v) {
  float x = v[0] * m[0] + v[1] * m[4] + v[2] * m[8];
  float y = v[0] * m[1] + v[1] * m[5] + v[2] * m[9];
  float z = v[0] * m[2] + v[1] * m[6] + v[2] * m[10];
  float w = v[0] * m[3] + v[1] * m[7] + v[2] * m[11];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
}
