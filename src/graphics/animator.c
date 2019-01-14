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
  animator->speed = 1;

  for (int i = 0; i < modelData->animationCount; i++) {
    Track track = {
      .time = 0.f,
      .speed = 1.f,
      .alpha = 1.f,
      .duration = 0.f,
      .priority = 0,
      .playing = false,
      .looping = false
    };

    ModelAnimation* animation = &modelData->animations[i];
    for (int j = 0; j < animation->samplerCount; j++) {
      ModelAnimationSampler* sampler = &modelData->animationSamplers[j];
      ModelAccessor* times = &modelData->accessors[sampler->times];
      if (times->hasMax) {
        track.duration = MAX(track.duration, times->max[0]);
      }
    }

    vec_push(&animator->tracks, track);
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
    track->time = 0;
    track->speed = 1;
    track->playing = false;
    track->looping = false;
  }
  animator->speed = 1;
}

void lovrAnimatorUpdate(Animator* animator, float dt) {
  Track* track; int i;
  vec_foreach_ptr(&animator->tracks, track, i) {
    if (track->playing) {
      track->time += dt * track->speed * animator->speed;

      if (track->looping) {
        track->time = fmodf(track->time, track->duration);
      } else if (track->time > track->duration || track->time < 0) {
        track->time = 0;
        track->playing = false;
      }
    }
  }
}

bool lovrAnimatorEvaluate(Animator* animator, int nodeIndex, mat4 transform) {
  ModelData* modelData = animator->modelData;
  float properties[3][4] = { { 0, 0, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 } };

  Track* track; int i;
  bool touched = false;
  vec_foreach_ptr(&animator->tracks, track, i) {
    ModelAnimation* animation = &modelData->animations[i];

    for (int j = 0; j < animation->channelCount; j++) {
      ModelAnimationChannel* channel = &animation->channels[j];

      if (!track->playing || channel->nodeIndex != nodeIndex) {
        continue;
      }

      float time = fmodf(track->time, track->duration);
      ModelAnimationSampler* sampler = &modelData->animationSamplers[channel->sampler];
      ModelAccessor* timeAccessor = &modelData->accessors[sampler->times];
      ModelAccessor* valueAccessor = &modelData->accessors[sampler->values];
      ModelView* timeView = &modelData->views[timeAccessor->view];
      ModelView* valueView = &modelData->views[valueAccessor->view];
      uint8_t* times = (uint8_t*) modelData->blobs[timeView->blob].data + timeView->offset + timeAccessor->offset;
      uint8_t* values = (uint8_t*) modelData->blobs[valueView->blob].data + valueView->offset + valueAccessor->offset;
      size_t timeStride = timeView->stride ? timeView->stride : sizeof(float);
      size_t valueStride = valueView->stride ? valueView->stride : (sizeof(float) * valueAccessor->components);

      int k = 0;
      while (k < timeAccessor->count) {
        float timestamp = *(float*) (times + timeStride * k);
        if (timestamp >= time) {
          break;
        } else {
          k++;
        }
      }

      float z = 0.f;
      float value[4];
      float next[4];
      if (k > 0 && k < timeAccessor->count - 1) {
        float t1 = *(float*) (times + timeStride * (k - 1));
        float t2 = *(float*) (times + timeStride * k);
        z = (time - t1) / (t2 - t1);
      }

      switch (channel->property) {
        case PROP_TRANSLATION:
        case PROP_SCALE:
          if (k == 0) {
            vec3_init(value, (float*) values);
          } else if (k >= timeAccessor->count) {
            vec3_init(value, (float*) ((uint8_t*) values + valueStride * (valueAccessor->count - 1)));
          } else {
            vec3_init(value, (float*) ((uint8_t*) values + valueStride * (k - 1)));
            vec3_init(next, (float*) ((uint8_t*) values + valueStride * k));

            switch (sampler->smoothing) {
              case SMOOTH_STEP:
                if (z >= .5) {
                  vec3_init(value, next);
                }
                break;
              case SMOOTH_LINEAR:
                vec3_lerp(value, next, z);
                break;
              case SMOOTH_CUBIC:
                lovrThrow("No spline interpolation yet");
                break;
            }
          }
          vec3_lerp(properties[channel->property], value, track->alpha);
          break;

        case PROP_ROTATION:
          if (k == 0) {
            quat_init(value, (float*) values);
          } else if (k >= timeAccessor->count) {
            quat_init(value, (float*) ((uint8_t*) values + valueStride * (valueAccessor->count - 1)));
          } else {
            quat_init(value, (float*) ((uint8_t*) values + valueStride * (k - 1)));
            quat_init(next, (float*) ((uint8_t*) values + valueStride * k));

            switch (sampler->smoothing) {
              case SMOOTH_STEP:
                if (z >= .5) {
                  quat_init(value, next);
                }
                break;
              case SMOOTH_LINEAR:
                quat_slerp(value, next, z);
                break;
              case SMOOTH_CUBIC:
                lovrThrow("No spline interpolation yet");
                break;
            }
          }
          quat_slerp(properties[channel->property], value, track->alpha);
          break;
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
  float duration = track->duration;

  while (time > track->duration) {
    time -= track->duration;
  }

  while (time < 0) {
    time += track->duration;
  }

  track->time = time;

  if (!track->looping) {
    track->time = MIN(track->time, track->duration);
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
  return track->duration;
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
