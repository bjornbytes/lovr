#include "matrix.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
   m0 m4 m8  m12
   m1 m5 m9  m13
   m2 m6 m10 m14
   m3 m7 m11 m15
*/

mat4 mat4_init() {
  mat4 matrix = malloc(16 * sizeof(float));
  return mat4_setIdentity(matrix);
}

void mat4_deinit(mat4 matrix) {
  free(matrix);
}

mat4 mat4_copy(mat4 source) {
  mat4 matrix = mat4_init();
  memcpy(matrix, source, 16 * sizeof(float));
  return matrix;
}

mat4 mat4_fromMat34(mat4 matrix, float (*source)[4]) {
  matrix[0] = source[0][0];
  matrix[1] = source[1][0];
  matrix[2] = source[2][0];
  matrix[3] = 0.f;
  matrix[4] = source[0][1];
  matrix[5] = source[1][1];
  matrix[6] = source[2][1];
  matrix[7] = 0.f;
  matrix[8] = source[0][2];
  matrix[9] = source[1][2];
  matrix[10] = source[2][2];
  matrix[11] = 0.f;
  matrix[12] = source[0][3];
  matrix[13] = source[1][3];
  matrix[14] = source[2][3];
  matrix[15] = 1.f;
  return matrix;
}

mat4 mat4_fromMat44(mat4 matrix, float (*source)[4]) {
  matrix[0] = source[0][0];
  matrix[1] = source[1][0];
  matrix[2] = source[2][0];
  matrix[3] = source[3][0];
  matrix[4] = source[0][1];
  matrix[5] = source[1][1];
  matrix[6] = source[2][1];
  matrix[7] = source[3][1];
  matrix[8] = source[0][2];
  matrix[9] = source[1][2];
  matrix[10] = source[2][2];
  matrix[11] = source[3][2];
  matrix[12] = source[0][3];
  matrix[13] = source[1][3];
  matrix[14] = source[2][3];
  matrix[15] = source[3][3];
  return matrix;
}

mat4 mat4_setIdentity(mat4 matrix) {
  memset(matrix, 0, 16 * sizeof(float));
  matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
  return matrix;
}

mat4 mat4_setTranslation(mat4 matrix, float x, float y, float z) {
  mat4_setIdentity(matrix);
  matrix[12] = x;
  matrix[13] = y;
  matrix[14] = z;
  return matrix;
}

mat4 mat4_setRotation(mat4 matrix, float angle, float ax, float ay, float az) {

  // Normalize rotation vector
  float len = sqrtf(ax * ax + ay * ay + az * az);
  if (len != 1 && len != 0) {
    len = 1 / len;
    ax *= len;
    ay *= len;
    az *= len;
  }

  // Convert angle-axis to quaternion
  float cos2 = cos(angle / 2.f);
  float sin2 = sin(angle / 2.f);
  float w = cos2;
  float x = sin2 * ax;
  float y = sin2 * ay;
  float z = sin2 * az;

  mat4_setIdentity(matrix);
  matrix[0] = 1 - 2 * y * y - 2 * z * z;
  matrix[1] = 2 * x * y + 2 * w * z;
  matrix[2] = 2 * x * z - 2 * w * y;
  matrix[4] = 2 * x * y - 2 * w * z;
  matrix[5] = 1 - 2 * x * x - 2 * z * z;
  matrix[6] = 2 * y * z + 2 * w * x;
  matrix[8] = 2 * x * z + 2 * w * y;
  matrix[9] = 2 * y * z - 2 * w * x;
  matrix[10] = 1 - 2 * x * x - 2 * y * y;
  return matrix;
}

mat4 mat4_setScale(mat4 matrix, float x, float y, float z) {
  mat4_setIdentity(matrix);
  matrix[0] = x;
  matrix[5] = y;
  matrix[10] = z;
  return matrix;
}

mat4 mat4_setProjection(mat4 matrix, float near, float far, float fov, float aspect) {
  float range = tan(fov * .5f) * near;
  float sx = (2.0f * near) / (range * aspect + range * aspect);
  float sy = near / range;
  float sz = -(far + near) / (far - near);
  float pz = (-2.0f * far * near) / (far - near);
  mat4_setIdentity(matrix);
  matrix[0] = sx;
  matrix[5] = sy;
  matrix[10] = sz;
  matrix[11] = -1.0f;
  matrix[14] = pz;
  matrix[15] = 0.0f;
  return matrix;
}

void mat4_getRotation(mat4 matrix, float* w, float* x, float* y, float* z) {
  float qw = sqrt(MAX(0, 1 + matrix[0] + matrix[5] + matrix[10])) / 2;
  float qx = sqrt(MAX(0, 1 + matrix[0] - matrix[5] - matrix[10])) / 2;
  float qy = sqrt(MAX(0, 1 - matrix[0] + matrix[5] - matrix[10])) / 2;
  float qz = sqrt(MAX(0, 1 - matrix[0] - matrix[5] + matrix[10])) / 2;
  qx = (matrix[9] - matrix[6]) > 0 ? -qx : qx;
  qy = (matrix[2] - matrix[8]) > 0 ? -qy : qy;
  qz = (matrix[4] - matrix[1]) > 0 ? -qz : qz;

  float s = sqrt(1 - qw * qw);
  s = s < .001 ? 1 : s;
  *w = 2 * acos(qw);
  *x = qx / s;
  *y = qy / s;
  *z = qz / s;
}

mat4 mat4_translate(mat4 matrix, float x, float y, float z) {
  float translation[16];
  mat4_setTranslation(translation, x, y, z);
  return mat4_multiply(matrix, translation);
}

mat4 mat4_rotate(mat4 matrix, float angle, float ax, float ay, float az) {
  float rotation[16];
  mat4_setRotation(rotation, angle, ax, ay, az);
  return mat4_multiply(matrix, rotation);
}

mat4 mat4_scale(mat4 matrix, float x, float y, float z) {
  float scale[16];
  mat4_setScale(scale, x, y, z);
  return mat4_multiply(matrix, scale);
}

// Modified from gl-matrix.c
mat4 mat4_multiply(mat4 a, mat4 b) {
  float a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3],
        a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7],
        a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11],
        a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15],

        b00 = b[0], b01 = b[1], b02 = b[2], b03 = b[3],
        b10 = b[4], b11 = b[5], b12 = b[6], b13 = b[7],
        b20 = b[8], b21 = b[9], b22 = b[10], b23 = b[11],
        b30 = b[12], b31 = b[13], b32 = b[14], b33 = b[15];

  a[0] = b00 * a00 + b01 * a10 + b02 * a20 + b03 * a30;
  a[1] = b00 * a01 + b01 * a11 + b02 * a21 + b03 * a31;
  a[2] = b00 * a02 + b01 * a12 + b02 * a22 + b03 * a32;
  a[3] = b00 * a03 + b01 * a13 + b02 * a23 + b03 * a33;
  a[4] = b10 * a00 + b11 * a10 + b12 * a20 + b13 * a30;
  a[5] = b10 * a01 + b11 * a11 + b12 * a21 + b13 * a31;
  a[6] = b10 * a02 + b11 * a12 + b12 * a22 + b13 * a32;
  a[7] = b10 * a03 + b11 * a13 + b12 * a23 + b13 * a33;
  a[8] = b20 * a00 + b21 * a10 + b22 * a20 + b23 * a30;
  a[9] = b20 * a01 + b21 * a11 + b22 * a21 + b23 * a31;
  a[10] = b20 * a02 + b21 * a12 + b22 * a22 + b23 * a32;
  a[11] = b20 * a03 + b21 * a13 + b22 * a23 + b23 * a33;
  a[12] = b30 * a00 + b31 * a10 + b32 * a20 + b33 * a30;
  a[13] = b30 * a01 + b31 * a11 + b32 * a21 + b33 * a31;
  a[14] = b30 * a02 + b31 * a12 + b32 * a22 + b33 * a32;
  a[15] = b30 * a03 + b31 * a13 + b32 * a23 + b33 * a33;

  return a;
}

void mat4_multiplyVector(mat4 m, float* v) {
  float v0 = v[0];
  float v1 = v[1];
  float v2 = v[2];
  float v3 = v[3];
  v[0] = v0 * m[0] + v1 * m[4] + v2 * m[8] + v3 * m[12];
  v[1] = v0 * m[1] + v1 * m[5] + v2 * m[9] + v3 * m[13];
  v[2] = v0 * m[2] + v1 * m[6] + v2 * m[10] + v3 * m[14];
  v[3] = v0 * m[3] + v1 * m[7] + v2 * m[11] + v3 * m[15];
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
