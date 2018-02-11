#include "math/math.h"

#pragma once

#define mat4_init mat4_set
mat4 mat4_set(mat4 m, mat4 n);
mat4 mat4_fromMat34(mat4 m, float (*n)[4]);
mat4 mat4_fromMat44(mat4 m, float (*n)[4]);
mat4 mat4_identity(mat4 m);
mat4 mat4_invert(mat4 m);
mat4 mat4_transpose(mat4 m);
mat4 mat4_multiply(mat4 m, mat4 n);
mat4 mat4_translate(mat4 m, float x, float y, float z);
mat4 mat4_rotate(mat4 m, float angle, float x, float y, float z);
mat4 mat4_rotateQuat(mat4 m, quat q);
mat4 mat4_scale(mat4 m, float x, float y, float z);
void mat4_getTransform(mat4 m, float* x, float* y, float* z, float* sx, float* sy, float* sz, float* angle, float* ax, float* ay, float* az);
mat4 mat4_setTransform(mat4 m, float x, float y, float z, float sx, float sy, float sz, float angle, float ax, float ay, float az);
mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float near, float far);
mat4 mat4_perspective(mat4 m, float near, float far, float fov, float aspect);
mat4 mat4_lookAt(mat4 m, vec3 from, vec3 to, vec3 up);
void mat4_transform(mat4 m, vec3 v);
void mat4_transformDirection(mat4 m, vec3 v);
