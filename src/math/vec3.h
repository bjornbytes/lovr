#ifndef LOVR_VECTOR_TYPES
#define LOVR_VECTOR_TYPES
typedef float* vec3;
#endif

vec3 vec3_init(float x, float y, float z);
vec3 vec3_set(vec3 v, vec3 u);
vec3 vec3_add(vec3 v, vec3 u);
vec3 vec3_sub(vec3 v, vec3 u);
vec3 vec3_mul(vec3 v, vec3 u);
vec3 vec3_div(vec3 v, vec3 u);
vec3 vec3_normalize(vec3 v);
float vec3_len(vec3 v);
float vec3_dist(vec3 v, vec3 u);
float vec3_angle(vec3 v, vec3 u);
float vec3_dot(vec3 v, vec3 u);
vec3 vec3_cross(vec3 v, vec3 u);
vec3 vec3_lerp(vec3 v, vec3 u, float t);
