#include "util.h"

#pragma once

#ifdef _WIN32
#define MAF_EXPORT __declspec(dllexport)
#else
#define MAF_EXPORT
#endif

typedef float* vec3;
typedef float* quat;
typedef float* mat4;

// vec3
MAF_EXPORT vec3 vec3_init(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_set(vec3 v, float x, float y, float z);
MAF_EXPORT vec3 vec3_add(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_sub(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_scale(vec3 v, float s);
MAF_EXPORT vec3 vec3_normalize(vec3 v);
MAF_EXPORT float vec3_length(vec3 v);
MAF_EXPORT float vec3_distance(vec3 v, vec3 u);
MAF_EXPORT float vec3_dot(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_cross(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_lerp(vec3 v, vec3 u, float t);
MAF_EXPORT vec3 vec3_min(vec3 v, vec3 u);
MAF_EXPORT vec3 vec3_max(vec3 v, vec3 u);

// quat
MAF_EXPORT quat quat_init(quat q, quat r);
MAF_EXPORT quat quat_set(quat q, float x, float y, float z, float w);
MAF_EXPORT quat quat_fromAngleAxis(quat q, float angle, float ax, float ay, float az);
MAF_EXPORT quat quat_fromMat4(quat q, mat4 m);
MAF_EXPORT quat quat_mul(quat q, quat r);
MAF_EXPORT quat quat_normalize(quat q);
MAF_EXPORT float quat_length(quat q);
MAF_EXPORT quat quat_slerp(quat q, quat r, float t);
MAF_EXPORT void quat_rotate(quat q, vec3 v);
MAF_EXPORT void quat_getAngleAxis(quat q, float* angle, float* x, float* y, float* z);
MAF_EXPORT quat quat_between(quat q, vec3 u, vec3 v);

// mat4
#define MAT4_IDENTITY { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
#define mat4_init mat4_set
MAF_EXPORT mat4 mat4_set(mat4 m, mat4 n);
MAF_EXPORT mat4 mat4_fromMat34(mat4 m, float (*n)[4]);
MAF_EXPORT mat4 mat4_fromMat44(mat4 m, float (*n)[4]);
MAF_EXPORT mat4 mat4_identity(mat4 m);
MAF_EXPORT mat4 mat4_invert(mat4 m);
MAF_EXPORT mat4 mat4_transpose(mat4 m);
MAF_EXPORT mat4 mat4_multiply(mat4 m, mat4 n);
MAF_EXPORT mat4 mat4_translate(mat4 m, float x, float y, float z);
MAF_EXPORT mat4 mat4_rotate(mat4 m, float angle, float x, float y, float z);
MAF_EXPORT mat4 mat4_rotateQuat(mat4 m, quat q);
MAF_EXPORT mat4 mat4_scale(mat4 m, float x, float y, float z);
MAF_EXPORT void mat4_getTransform(mat4 m, float* x, float* y, float* z, float* sx, float* sy, float* sz, float* angle, float* ax, float* ay, float* az);
MAF_EXPORT mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float near, float far);
MAF_EXPORT mat4 mat4_perspective(mat4 m, float near, float far, float fov, float aspect);
MAF_EXPORT mat4 mat4_lookAt(mat4 m, vec3 from, vec3 to, vec3 up);
MAF_EXPORT void mat4_transform(mat4 m, float* x, float* y, float* z);
MAF_EXPORT void mat4_transformDirection(mat4 m, float* x, float* y, float* z);

#ifdef LOVR_USE_SSE
MAF_EXPORT mat4 mat4_invertPose(mat4 m);
#else
#define mat4_invertPose mat4_invert
#endif
