#include "graphics/animator.h"

Animator* lovrAnimatorCreate(AnimationData* animationData) {
  Animator* animator = lovrAlloc(sizeof(Animator), lovrAnimatorDestroy);
  if (!animator) return NULL;

  animator->animationData = animationData;

  return animator;
}

void lovrAnimatorDestroy(const Ref* ref) {
  Animator* animator = containerof(ref, Animator);
  free(animator);
}

int lovrAnimatorGetAnimationCount(Animator* animator) {
  return animator->animationData->animations.length;
}
