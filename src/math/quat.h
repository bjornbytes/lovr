#include "math/math.h"

quat quat_init(float x, float y, float z, float w);
quat quat_fromAngleAxis(quat q, float angle, vec3 axis);
quat quat_fromDirection(quat q, vec3 forward, vec3 up);
quat quat_set(quat q, quat r);
quat quat_multiply(quat q, quat r);
quat quat_normalize(quat q);
float quat_length(quat q);
quat quat_slerp(quat q, quat r, float t);
quat quat_between(quat q, vec3 u, vec3 v);
void quat_toAngleAxis(quat q, float* angle, float* x, float* y, float* z);
