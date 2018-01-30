#include "data/model.h"
#include "math/mat4.h"
#include "util.h"
#include "lib/map/map.h"
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
  ModelData* modelData;
  map_track_t trackMap;
  vec_void_t trackList;
  float speed;
} Animator;

Animator* lovrAnimatorCreate(ModelData* modelData);
void lovrAnimatorDestroy(const Ref* ref);
void lovrAnimatorReset(Animator* animator);
void lovrAnimatorUpdate(Animator* animator, float dt);
bool lovrAnimatorEvaluate(Animator* animator, const char* bone, mat4 transform);
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
