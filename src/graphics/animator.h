#include "data/modelData.h"
#include "util.h"
#include "lib/math.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

typedef struct {
  float time;
  float speed;
  float alpha;
  float duration;
  int priority;
  bool playing;
  bool looping;
} Track;

typedef vec_t(Track) vec_track_t;

typedef struct {
  Ref ref;
  ModelData* modelData;
  vec_track_t tracks;
  float speed;
} Animator;

Animator* lovrAnimatorInit(Animator* animator, ModelData* modelData);
#define lovrAnimatorCreate(...) lovrAnimatorInit(lovrAlloc(Animator), __VA_ARGS__)
void lovrAnimatorDestroy(void* ref);
void lovrAnimatorReset(Animator* animator);
void lovrAnimatorUpdate(Animator* animator, float dt);
bool lovrAnimatorEvaluate(Animator* animator, int nodeIndex, mat4 transform);
int lovrAnimatorGetAnimationCount(Animator* animator);
void lovrAnimatorPlay(Animator* animator, int animation);
void lovrAnimatorStop(Animator* animator, int animation);
void lovrAnimatorPause(Animator* animator, int animation);
void lovrAnimatorResume(Animator* animator, int animation);
void lovrAnimatorSeek(Animator* animator, int animation, float time);
float lovrAnimatorTell(Animator* animator, int animation);
float lovrAnimatorGetAlpha(Animator* animator, int animation);
void lovrAnimatorSetAlpha(Animator* animator, int animation, float alpha);
float lovrAnimatorGetDuration(Animator* animator, int animation);
bool lovrAnimatorIsPlaying(Animator* animator, int animation);
bool lovrAnimatorIsLooping(Animator* animator, int animation);
void lovrAnimatorSetLooping(Animator* animator, int animation, bool loop);
int lovrAnimatorGetPriority(Animator* animator, int animation);
void lovrAnimatorSetPriority(Animator* animator, int animation, int priority);
float lovrAnimatorGetSpeed(Animator* animator, int animation);
void lovrAnimatorSetSpeed(Animator* animator, int animation, float speed);
