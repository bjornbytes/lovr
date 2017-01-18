#include "math/math.h"

mat4 mat4_init();
mat4 mat4_fromMat34(mat4 m, float (*n)[4]);
mat4 mat4_fromMat44(mat4 m, float (*n)[4]);
mat4 mat4_set(mat4 m, mat4 n);
mat4 mat4_identity(mat4 m);
mat4 mat4_transpose(mat4 m);
mat4 mat4_invert(mat4 m);
mat4 mat4_multiply(mat4 m, mat4 n);
mat4 mat4_translate(mat4 m, float x, float y, float z);
mat4 mat4_scale(mat4 m, float x, float y, float z);
mat4 mat4_rotate(mat4 m, quat q);
mat4 mat4_orthographic(mat4 m, float left, float right, float top, float bottom, float near, float far);
mat4 mat4_perspective(mat4 m, float near, float far, float fov, float aspect);
quat mat4_toQuat(mat4 m, quat q);
