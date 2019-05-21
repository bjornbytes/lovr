#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct ModelData;

typedef struct {
  float time;
  float speed;
  float alpha;
  int32_t priority;
  bool playing;
  bool looping;
} Track;

typedef vec_t(Track) vec_track_t;

typedef struct Animator {
  struct ModelData* data;
  map_t(uint32_t) animations;
  vec_track_t tracks;
  float speed;
} Animator;

Animator* lovrAnimatorInit(Animator* animator, struct ModelData* modelData);
#define lovrAnimatorCreate(...) lovrAnimatorInit(lovrAlloc(Animator), __VA_ARGS__)
void lovrAnimatorDestroy(void* ref);
void lovrAnimatorReset(Animator* animator);
void lovrAnimatorUpdate(Animator* animator, float dt);
bool lovrAnimatorEvaluate(Animator* animator, uint32_t nodeIndex, float* transform);
uint32_t lovrAnimatorGetAnimationCount(Animator* animator);
uint32_t* lovrAnimatorGetAnimationIndex(Animator* animator, const char* name);
const char* lovrAnimatorGetAnimationName(Animator* animator, uint32_t index);
void lovrAnimatorPlay(Animator* animator, uint32_t animation);
void lovrAnimatorStop(Animator* animator, uint32_t animation);
void lovrAnimatorPause(Animator* animator, uint32_t animation);
void lovrAnimatorResume(Animator* animator, uint32_t animation);
void lovrAnimatorSeek(Animator* animator, uint32_t animation, float time);
float lovrAnimatorTell(Animator* animator, uint32_t animation);
float lovrAnimatorGetAlpha(Animator* animator, uint32_t animation);
void lovrAnimatorSetAlpha(Animator* animator, uint32_t animation, float alpha);
float lovrAnimatorGetDuration(Animator* animator, uint32_t animation);
bool lovrAnimatorIsPlaying(Animator* animator, uint32_t animation);
bool lovrAnimatorIsLooping(Animator* animator, uint32_t animation);
void lovrAnimatorSetLooping(Animator* animator, uint32_t animation, bool loop);
int32_t lovrAnimatorGetPriority(Animator* animator, uint32_t animation);
void lovrAnimatorSetPriority(Animator* animator, uint32_t animation, int32_t priority);
float lovrAnimatorGetSpeed(Animator* animator, uint32_t animation);
void lovrAnimatorSetSpeed(Animator* animator, uint32_t animation, float speed);
