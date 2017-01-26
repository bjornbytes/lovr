#include "math/math.h"

#pragma once

quat quat_init(quat q, quat r);
quat quat_set(quat q, float x, float y, float z, float w);
quat quat_fromAngleAxis(quat q, float angle, vec3 axis);
quat quat_fromMat4(quat q, mat4 m);
quat quat_normalize(quat q);
float quat_length(quat q);
void quat_rotate(quat q, vec3 v);
void quat_getAngleAxis(quat q, float* angle, float* x, float* y, float* z);
