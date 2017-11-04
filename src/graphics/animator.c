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
      .animation = &animationData->animations.data[i],
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

void lovrAnimatorUpdate(Animator* animator, float dt) {
  map_iter_t iter = map_iter(&animator->timeline);
  const char* key;
  while ((key = map_next(&animator->timeline, &iter)) != NULL) {
    Track* track = map_get(&animator->timeline, key);
    track->time += dt * track->speed * animator->speed;
  }
}

int lovrAnimatorGetAnimationCount(Animator* animator) {
  return animator->animationData->animations.length;
}

const char* lovrAnimatorGetAnimationName(Animator* animator, int index) {
  if (index < 0 || index >= animator->animationData->animations.length) {
    return NULL;
  }

  return animator->animationData->animations.data[index].name;
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

void lovrAnimatorSeek(Animator* animator, const char* animation, float time) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->time = time;
}

float lovrAnimatorTell(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->time;
}

float lovrAnimatorGetDuration(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->animation->duration;
}

float lovrAnimatorGetSpeed(Animator* animator, const char* animation) {
  if (!animation) {
    return animator->speed;
  }

  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->speed;
}

void lovrAnimatorSetSpeed(Animator* animator, const char* animation, float speed) {
  if (!animation) {
    animator->speed = speed;
  }

  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->speed = speed;
}
