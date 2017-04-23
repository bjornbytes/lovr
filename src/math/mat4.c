#include "math/mat4.h"
#include "math/quat.h"
#include "math/vec3.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// m0 m4 m8  m12
// m1 m5 m9  m13
// m2 m6 m10 m14
// m3 m7 m11 m15

mat4 mat4_set(mat4 m, mat4 n) {
  return memcpy(m, n, 16 * sizeof(float));
}

mat4 mat4_fromMat34(mat4 m, float (*n)[4]) {
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

mat4 mat4_fromMat44(mat4 m, float (*n)[4]) {
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

mat4 mat4_identity(mat4 m) {
  memset(m, 0, 16 * sizeof(float));
  m[0] = m[5] = m[10] = m[15] = 1.f;
  return m;
}

// Modified from gl-matrix.c
mat4 mat4_invert(mat4 m) {
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

  if (!d) { return NULL; }
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

// Modified from gl-matrix.c
mat4 mat4_multiply(mat4 m, mat4 n) {
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

mat4 mat4_translate(mat4 m, float x, float y, float z) {
  m[12] = m[0] * x + m[4] * y + m[8] * z + m[12];
  m[13] = m[1] * x + m[5] * y + m[9] * z + m[13];
  m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
  m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
  return m;
}

mat4 mat4_rotate(mat4 m, float angle, float x, float y, float z) {
  float q[4];
  float v[3];
  quat_fromAngleAxis(q, angle, vec3_set(v, x, y, z));
  return mat4_rotateQuat(m, q);
}

mat4 mat4_rotateQuat(mat4 m, quat q) {
  float x = q[0];
  float y = q[1];
  float z = q[2];
  float w = q[3];
  float rotation[16];
  mat4_identity(rotation);
  rotation[0] = 1 - 2 * y * y - 2 * z * z;
  rotation[1] = 2 * x * y + 2 * w * z;
  rotation[2] = 2 * x * z - 2 * w * y;
  rotation[4] = 2 * x * y - 2 * w * z;
  rotation[5] = 1 - 2 * x * x - 2 * z * z;
  rotation[6] = 2 * y * z + 2 * w * x;
  rotation[8] = 2 * x * z + 2 * w * y;
  rotation[9] = 2 * y * z - 2 * w * x;
  rotation[10] = 1 - 2 * x * x - 2 * y * y;
  return mat4_multiply(m, rotation);
}

mat4 mat4_scale(mat4 m, float x, float y, float z) {
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

mat4 mat4_setTransform(mat4 m, float x, float y, float z, float s, float angle, float ax, float ay, float az) {
  mat4_identity(m);
  mat4_translate(m, x, y, z);
  mat4_scale(m, s, s, s);
  return mat4_rotate(m, angle, ax, ay, az);
}

mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float near, float far) {
  float rl = right - left;
  float tb = top - bottom;
  float fn = far - near;
  mat4_identity(m);
  m[0] = 2 / rl;
  m[5] = 2 / tb;
  m[10] = -2 / fn;
  m[12] = -(left + right) / rl;
  m[13] = -(top + bottom) / tb;
  m[14] = -(far + near) / fn;
  m[15] = 1;
  return m;
}

mat4 mat4_perspective(mat4 m, float near, float far, float fovy, float aspect) {
  float range = tan(fovy * .5f) * near;
  float sx = (2.0f * near) / (range * aspect + range * aspect);
  float sy = near / range;
  float sz = -(far + near) / (far - near);
  float pz = (-2.0f * far * near) / (far - near);
  mat4_identity(m);
  m[0] = sx;
  m[5] = sy;
  m[10] = sz;
  m[11] = -1.0f;
  m[14] = pz;
  m[15] = 0.0f;
  return m;
}

void mat4_transform(mat4 m, vec3 v) {
  vec3_set(v,
    v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12],
    v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13],
    v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14]
  );
}

void mat4_transformDirection(mat4 m, vec3 v) {
  vec3_set(v,
    v[0] * m[0] + v[1] * m[4] + v[2] * m[8],
    v[0] * m[1] + v[1] * m[5] + v[2] * m[9],
    v[0] * m[2] + v[1] * m[6] + v[2] * m[10]
  );
}
