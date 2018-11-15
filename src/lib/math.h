#pragma once

typedef float* vec3;
typedef float* quat;
typedef float* mat4;

// vec3
vec3 vec3_init(vec3 v, vec3 u);
vec3 vec3_set(vec3 v, float x, float y, float z);
vec3 vec3_add(vec3 v, vec3 u);
vec3 vec3_scale(vec3 v, float s);
vec3 vec3_normalize(vec3 v);
float vec3_length(vec3 v);
float vec3_dot(vec3 v, vec3 u);
vec3 vec3_cross(vec3 v, vec3 u);
vec3 vec3_lerp(vec3 v, vec3 u, float t);

// quat
quat quat_init(quat q, quat r);
quat quat_set(quat q, float x, float y, float z, float w);
quat quat_fromAngleAxis(quat q, float angle, vec3 axis);
quat quat_fromMat4(quat q, mat4 m);
quat quat_normalize(quat q);
float quat_length(quat q);
quat quat_slerp(quat q, quat r, float t);
void quat_rotate(quat q, vec3 v);
void quat_getAngleAxis(quat q, float* angle, float* x, float* y, float* z);

// mat4
#define MAT4_IDENTITY { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
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
void mat4_getPose(mat4 m, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
void mat4_getTransform(mat4 m, float* x, float* y, float* z, float* sx, float* sy, float* sz, float* angle, float* ax, float* ay, float* az);
mat4 mat4_setTransform(mat4 m, float x, float y, float z, float sx, float sy, float sz, float angle, float ax, float ay, float az);
mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float near, float far);
mat4 mat4_perspective(mat4 m, float near, float far, float fov, float aspect);
mat4 mat4_lookAt(mat4 m, vec3 from, vec3 to, vec3 up);
void mat4_transform(mat4 m, float* x, float* y, float* z);
void mat4_transformDirection(mat4 m, float* x, float* y, float* z);

#ifdef LOVR_USE_SSE
mat4 mat4_invertPose(mat4 m);
#else
#define mat4_invertPose mat4_invert
#endif
