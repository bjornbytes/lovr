#include "util.h"
#include "math/math.h"
#include <stdbool.h>

#pragma once

typedef struct Transform {
  Ref ref;
  float matrix[16];
  float inverse[16];
  bool isDirty;
} Transform;

Transform* lovrTransformCreate(mat4 transfrom);
void lovrTransformGetMatrix(Transform* transform, mat4 m);
void lovrTransformSetMatrix(Transform* transform, mat4 m);
void lovrTransformApply(Transform* transform, Transform* other);
mat4 lovrTransformInverse(Transform* transform);
void lovrTransformOrigin(Transform* transform);
void lovrTransformTranslate(Transform* transform, float x, float y, float z);
void lovrTransformRotate(Transform* transform, float angle, float x, float y, float z);
void lovrTransformScale(Transform* transform, float x, float y, float z);
void lovrTransformTransformPoint(Transform* transform, vec3 point);
void lovrTransformInverseTransformPoint(Transform* transform, vec3 point);
