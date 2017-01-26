#include "math/math.h"

#pragma once

vec3 vec3_init(vec3 v, vec3 u);
vec3 vec3_set(vec3 v, float x, float y, float z);
vec3 vec3_add(vec3 v, vec3 u);
vec3 vec3_scale(vec3 v, float s);
vec3 vec3_normalize(vec3 v);
float vec3_length(vec3 v);
float vec3_dot(vec3 v, vec3 u);
vec3 vec3_cross(vec3 v, vec3 u);
