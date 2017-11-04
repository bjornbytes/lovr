#include "graphics/animator.h"

static Track* lovrAnimatorEnsureTrack(Animator* animator, const char* animation) {
  Track* track = map_get(&animator->timeline, animation);
  lovrAssert(track, "Animation '%s' does not exist", animation);
  return track;
}

Animator* lovrAnimatorCreate(AnimationData* animationData) {
  Animator* animator = lovrAlloc(sizeof(Animator), lovrAnimatorDestroy);
  if (!animator) return NULL;

  animator->animationData = animationData;
  map_init(&animator->timeline);
  animator->speed = 1;

  for (int i = 0; i < animationData->animations.length; i++) {
    Track track = {
      .time = 0,
      .speed = 1,
      .playing = false,
      .looping = false
    };

    map_set(&animator->timeline, animationData->animations.data[i].name, track);
  }

  return animator;
}

void lovrAnimatorDestroy(const Ref* ref) {
  Animator* animator = containerof(ref, Animator);
  map_deinit(&animator->timeline);
  free(animator);
}

int lovrAnimatorGetAnimationCount(Animator* animator) {
  return animator->animationData->animations.length;
}

void lovrAnimatorUpdate(Animator* animator, float dt) {
  map_iter_t iter = map_iter(&animator->timeline);
  const char* key;
  while ((key = map_next(&animator->timeline, &iter)) != NULL) {
    Track* track = map_get(&animator->timeline, key);
    track->time += dt * track->speed * animator->speed;
  }
}

void lovrAnimatorPlay(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->playing = true;
  track->time = 0;
}

void lovrAnimatorStop(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->playing = false;
  track->time = 0;
}

void lovrAnimatorPause(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->playing = false;
}

void lovrAnimatorResume(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->playing = true;
}
