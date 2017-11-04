#include "graphics/animator.h"
#include "math/math.h"
#include <math.h>

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

    if (track->looping) {
      track->time = fmodf(track->time, track->animation->duration);
    } else if (track->time > track->animation->duration) {
      track->time = 0;
      track->playing = false;
    }
  }
}

void lovrAnimatorReset(Animator* animator) {
  map_iter_t iter = map_iter(&animator->timeline);
  const char* key;
  while ((key = map_next(&animator->timeline, &iter)) != NULL) {
    Track* track = map_get(&animator->timeline, key);
    track->time = 0;
    track->speed = 1;
    track->playing = true;
    track->looping = false;
  }
  animator->speed = 1;
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
  if (!track->looping) {
    track->time = MIN(track->time, track->animation->duration);
    track->time = MAX(track->time, 0);
  }
}

float lovrAnimatorTell(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->time;
}

float lovrAnimatorGetAlpha(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->alpha;
}

void lovrAnimatorSetAlpha(Animator* animator, const char* animation, float alpha) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->alpha = alpha;
}

float lovrAnimatorGetDuration(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->animation->duration;
}

bool lovrAnimatorIsPlaying(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->playing;
}

bool lovrAnimatorIsLooping(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->looping;
}

void lovrAnimatorSetLooping(Animator* animator, const char* animation, bool looping) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->looping = looping;
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
