#include "math/math.h"

vec3 vec3_init(vec3 v, vec3 u);
vec3 vec3_set(vec3 v, float x, float y, float z);
vec3 vec3_add(vec3 v, vec3 u);
vec3 vec3_sub(vec3 v, vec3 u);
vec3 vec3_mul(vec3 v, vec3 u);
vec3 vec3_div(vec3 v, vec3 u);
vec3 vec3_scale(vec3 v, float s);
vec3 vec3_normalize(vec3 v);
float vec3_length(vec3 v);
float vec3_distance(vec3 v, vec3 u);
float vec3_angle(vec3 v, vec3 u);
float vec3_dot(vec3 v, vec3 u);
vec3 vec3_cross(vec3 v, vec3 u);
vec3 vec3_rotate(vec3 v, quat q);
vec3 vec3_transform(vec3 v, mat4 m);
vec3 vec3_lerp(vec3 v, vec3 u, float t);
