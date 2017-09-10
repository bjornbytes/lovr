#include "transform.h"
#include "math/mat4.h"

Transform* lovrTransformCreate(mat4 transfrom) {
  Transform* transform = lovrAlloc(sizeof(Transform), lovrTransformDestroy);
  if (!transform) return NULL;

  transform->isDirty = 1;

  if (transfrom) {
    mat4_set(transform->matrix, transfrom);
  } else {
    mat4_identity(transform->matrix);
  }

  return transform;
}

void lovrTransformDestroy(const Ref* ref) {
  Transform* transform = containerof(ref, Transform);
  free(transform);
}

void lovrTransformGetMatrix(Transform* transform, mat4 m) {
  mat4_set(m, transform->matrix);
}

void lovrTransformSetMatrix(Transform* transform, mat4 m) {
  transform->isDirty = 1;
  mat4_set(transform->matrix, m);
}

mat4 lovrTransformInverse(Transform* transform) {
  if (transform->isDirty) {
    transform->isDirty = 0;
    mat4_invert(mat4_set(transform->inverse, transform->matrix));
  }

  return transform->inverse;
}

void lovrTransformApply(Transform* transform, Transform* other) {
  transform->isDirty = 1;
  mat4_multiply(transform->matrix, other->matrix);
}

void lovrTransformOrigin(Transform* transform) {
  transform->isDirty = 1;
  mat4_identity(transform->matrix);
}

void lovrTransformTranslate(Transform* transform, float x, float y, float z) {
  transform->isDirty = 1;
  mat4_translate(transform->matrix, x, y, z);
}

void lovrTransformRotate(Transform* transform, float angle, float x, float y, float z) {
  transform->isDirty = 1;
  mat4_rotate(transform->matrix, angle, x, y, z);
}

void lovrTransformScale(Transform* transform, float x, float y, float z) {
  transform->isDirty = 1;
  mat4_scale(transform->matrix, x, y, z);
}

void lovrTransformTransformPoint(Transform* transform, vec3 point) {
  mat4_transform(transform->matrix, point);
}

void lovrTransformInverseTransformPoint(Transform* transform, vec3 point) {
  mat4_transform(lovrTransformInverse(transform), point);
}
