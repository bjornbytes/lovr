#include "graphics/animator.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static int trackSortCallback(const void* a, const void* b) {
  return ((Track*) a)->priority < ((Track*) b)->priority;
}

Animator* lovrAnimatorInit(Animator* animator, ModelData* modelData) {
  lovrRetain(modelData);
  animator->modelData = modelData;
  vec_init(&animator->tracks);
  vec_reserve(&animator->tracks, modelData->animationCount);
  animator->speed = 1.f;

  for (int i = 0; i < modelData->animationCount; i++) {
    vec_push(&animator->tracks, ((Track) {
      .time = 0.f,
      .speed = 1.f,
      .alpha = 1.f,
      .priority = 0,
      .playing = false,
      .looping = false
    }));
  }

  return animator;
}

void lovrAnimatorDestroy(void* ref) {
  Animator* animator = ref;
  lovrRelease(animator->modelData);
  vec_deinit(&animator->tracks);
}

void lovrAnimatorReset(Animator* animator) {
  Track* track; int i;
  vec_foreach_ptr(&animator->tracks, track, i) {
    track->time = 0.f;
    track->speed = 1.f;
    track->playing = false;
    track->looping = false;
  }
  animator->speed = 1.f;
}

void lovrAnimatorUpdate(Animator* animator, float dt) {
  Track* track; int i;
  vec_foreach_ptr(&animator->tracks, track, i) {
    if (track->playing) {
      track->time += dt * track->speed * animator->speed;
      float duration = animator->modelData->animations[i].duration;

      if (track->looping) {
        track->time = fmodf(track->time, duration);
      } else if (track->time > duration || track->time < 0) {
        track->time = 0;
        track->playing = false;
      }
    }
  }
}

bool lovrAnimatorEvaluate(Animator* animator, int nodeIndex, mat4 transform) {
  ModelData* modelData = animator->modelData;
  float properties[3][4] = { { 0, 0, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 } };
  bool touched = false;

  for (int i = 0; i < modelData->animationCount; i++) {
    ModelAnimation* animation = &modelData->animations[i];

    for (int j = 0; j < animation->channelCount; j++) {
      ModelAnimationChannel* channel = &animation->channels[j];
      if (channel->nodeIndex != nodeIndex) {
        continue;
      }

      Track* track = &animator->tracks.data[i];
      if (!track->playing || track->alpha == 0.f) {
        continue;
      }

      float duration = animator->modelData->animations[i].duration;
      float time = fmodf(track->time, duration);
      int k = 0;

      while (k < channel->keyframeCount && channel->times[k] < time) {
        k++;
      }

      float value[4];
      bool rotate = channel->property == PROP_ROTATION;
      int n = 3 + rotate;
      float* (*lerp)(float* a, float* b, float t) = rotate ? quat_slerp : vec3_lerp;

      if (k > 0 && k < channel->keyframeCount) {
        float t1 = channel->times[k - 1];
        float t2 = channel->times[k];
        float z = (time - t1) / (t2 - t1);
        float next[4];

        memcpy(value, channel->data + (k - 1) * n, n * sizeof(float));
        memcpy(next, channel->data + k * n, n * sizeof(float));

        switch (channel->smoothing) {
          case SMOOTH_STEP:
            if (z >= .5) {
              memcpy(value, next, n * sizeof(float));
            }
            break;
          case SMOOTH_LINEAR: lerp(value, next, z); break;
          case SMOOTH_CUBIC: lovrThrow("No spline interpolation yet"); break;
        }
      } else {
        memcpy(value, channel->data + CLAMP(k, 0, channel->keyframeCount - 1) * n, n * sizeof(float));
      }

      if (track->alpha == 1.f) {
        memcpy(properties[channel->property], value, n * sizeof(float));
      } else {
        lerp(properties[channel->property], value, track->alpha);
      }

      touched = true;
    }
  }

  if (touched) {
    vec3 T = properties[PROP_TRANSLATION];
    quat R = properties[PROP_ROTATION];
    vec3 S = properties[PROP_SCALE];
    mat4_translate(transform, T[0], T[1], T[2]);
    mat4_rotateQuat(transform, R);
    mat4_scale(transform, S[0], S[1], S[2]);
  }

  return touched;
}

int lovrAnimatorGetAnimationCount(Animator* animator) {
  return animator->modelData->animationCount;
}

void lovrAnimatorPlay(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = true;
  track->time = 0;
}

void lovrAnimatorStop(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = false;
  track->time = 0;
}

void lovrAnimatorPause(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = false;
}

void lovrAnimatorResume(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = true;
}

void lovrAnimatorSeek(Animator* animator, int animation, float time) {
  Track* track = &animator->tracks.data[animation];
  float duration = animator->modelData->animations[animation].duration;

  while (time > duration) {
    time -= duration;
  }

  while (time < 0) {
    time += duration;
  }

  track->time = time;

  if (!track->looping) {
    track->time = MIN(track->time, duration);
    track->time = MAX(track->time, 0);
  }
}

float lovrAnimatorTell(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return track->time;
}

float lovrAnimatorGetAlpha(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return track->alpha;
}

void lovrAnimatorSetAlpha(Animator* animator, int animation, float alpha) {
  Track* track = &animator->tracks.data[animation];
  track->alpha = alpha;
}

float lovrAnimatorGetDuration(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return animator->modelData->animations[animation].duration;
}

bool lovrAnimatorIsPlaying(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return track->playing;
}

bool lovrAnimatorIsLooping(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return track->looping;
}

void lovrAnimatorSetLooping(Animator* animator, int animation, bool loop) {
  Track* track = &animator->tracks.data[animation];
  track->looping = loop;
}

int lovrAnimatorGetPriority(Animator* animator, int animation) {
  Track* track = &animator->tracks.data[animation];
  return track->priority;
}

void lovrAnimatorSetPriority(Animator* animator, int animation, int priority) {
  Track* track = &animator->tracks.data[animation];
  track->priority = priority;
  vec_sort(&animator->tracks, trackSortCallback);
}

float lovrAnimatorGetSpeed(Animator* animator, int animation) {
  if (animation < 0) {
    return animator->speed;
  }

  Track* track = &animator->tracks.data[animation];
  return track->speed;
}

void lovrAnimatorSetSpeed(Animator* animator, int animation, float speed) {
  if (animation < 0) {
    animator->speed = speed;
  }

  Track* track = &animator->tracks.data[animation];
  track->speed = speed;
}
