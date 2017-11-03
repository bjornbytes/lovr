#include "util.h"
#include "loaders/animation.h"

#pragma once

typedef struct {
  Ref ref;
  AnimationData* animationData;
} Animator;

Animator* lovrAnimatorCreate(AnimationData* animationData);
void lovrAnimatorDestroy(const Ref* ref);
int lovrAnimatorGetAnimationCount(Animator* animator);
