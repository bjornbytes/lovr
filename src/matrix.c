#include "matrix.h"
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

mat4 mat4_copy(mat4 source) {
  mat4 matrix = mat4_init();
  memcpy(matrix, source, 16 * sizeof(float));
  return matrix;
}

void mat4_deinit(mat4 matrix) {
  free(matrix);
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

mat4 mat4_setRotation(mat4 matrix, float w, float x, float y, float z) {
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

mat4 mat4_translate(mat4 matrix, float x, float y, float z) {
  float translation[16];
  mat4_setTranslation(translation, x, y, z);
  return mat4_multiply(matrix, translation);
}

mat4 mat4_rotate(mat4 matrix, float w, float x, float y, float z) {
  float rotation[16];
  mat4_setRotation(rotation, w, x, y, z);
  return mat4_multiply(matrix, rotation);
}

mat4 mat4_scale(mat4 matrix, float x, float y, float z) {
  float scale[16];
  mat4_setScale(scale, x, y, z);
  return mat4_multiply(matrix, scale);
}

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
