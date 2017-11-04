#include "loaders/animation.h"
#include "lib/map/map.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  Animation* animation;
  float time;
  float speed;
  float alpha;
  int priority;
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
void lovrAnimatorUpdate(Animator* animator, float dt);
void lovrAnimatorReset(Animator* animator);
int lovrAnimatorGetAnimationCount(Animator* animator);
const char* lovrAnimatorGetAnimationName(Animator* animator, int index);
void lovrAnimatorPlay(Animator* animator, const char* animation);
void lovrAnimatorStop(Animator* animator, const char* animation);
void lovrAnimatorPause(Animator* animator, const char* animation);
void lovrAnimatorResume(Animator* animator, const char* animation);
void lovrAnimatorSeek(Animator* animator, const char* animation, float time);
float lovrAnimatorTell(Animator* animator, const char* animation);
float lovrAnimatorGetAlpha(Animator* animator, const char* animation);
void lovrAnimatorSetAlpha(Animator* animator, const char* animation, float alpha);
float lovrAnimatorGetDuration(Animator* animator, const char* animation);
bool lovrAnimatorIsPlaying(Animator* animator, const char* animation);
bool lovrAnimatorIsLooping(Animator* animator, const char* animation);
void lovrAnimatorSetLooping(Animator* animator, const char* animation, bool looping);
int lovrAnimatorGetPriority(Animator* animator, const char* animation);
void lovrAnimatorSetPriority(Animator* animator, const char* animation, int priority);
float lovrAnimatorGetSpeed(Animator* animator, const char* animation);
void lovrAnimatorSetSpeed(Animator* animator, const char* animation, float speed);
