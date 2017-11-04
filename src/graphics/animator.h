#include "loaders/animation.h"
#include "lib/map/map.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  float time;
  float speed;
  bool playing;
  bool looping;
} Track;

typedef map_t(Track) map_track_t;

typedef struct {
  Ref ref;
  AnimationData* animationData;
  map_track_t timeline;
  float speed;
} Animator;

Animator* lovrAnimatorCreate(AnimationData* animationData);
void lovrAnimatorDestroy(const Ref* ref);
int lovrAnimatorGetAnimationCount(Animator* animator);
void lovrAnimatorUpdate(Animator* animator, float dt);
