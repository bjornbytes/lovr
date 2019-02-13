#include "graphics/animator.h"
#include <math.h>
#include <stdlib.h>

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
      .time = 0,
      .speed = 1,
      .priority = 0,
      .alpha = 1,
      .playing = false,
      .looping = false
    };

    // TODO to get track duration: loop over samplers in animation, check max property of times
    // accessor (it's guaranteed to be there by the spec), and keep MAX-ing that.

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
  float translation[3] = { 0, 0, 0 };
  float rotation[4] = { 0, 0, 0, 1 };
  float scale[3] = { 1, 1, 1 };

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

      int k = 0;
      while (k < timeAccessor->count) {
        float timestamp = *(float*) (times + timeView->stride * k);
        if (time >= timestamp) {
          break;
        } else {
          k++;
        }
      }

      float t1 = *(float*) (times + timeView->stride * k);
      float t2 = *(float*) (times + timeView->stride * (k + 1));
      float z = (time - t1) / (t2 - t1);
      float value[4];
      float next[4];

      switch (channel->property) {
        case PROP_TRANSLATION:
          if (k == 0) {
            vec3_init(value, (float*) values);
          } else if (k >= timeAccessor->count) {
            vec3_init(value, (float*) values + valueView->stride * (valueAccessor->count - 1));
          } else {
            vec3_init(value, (float*) values + valueView->stride * k);
            vec3_init(next, (float*) values + valueView->stride * (k + 1));
            vec3_lerp(value, next, z);
          }
          vec3_lerp(translation, value, track->alpha);
          touched = true;
          break;
        case PROP_ROTATION:
          if (k == 0) {
            quat_init(value, (float*) values);
          } else if (k >= timeAccessor->count) {
            quat_init(value, (float*) values + valueView->stride * (valueAccessor->count - 1));
          } else {
            quat_init(value, (float*) values + valueView->stride * k);
            quat_init(next, (float*) values + valueView->stride * (k + 1));
            quat_slerp(value, next, z);
          }
          quat_slerp(rotation, value, track->alpha);
          touched = true;
          break;
        case PROP_SCALE:
          if (k == 0) {
            vec3_init(value, (float*) values);
          } else if (k >= timeAccessor->count) {
            vec3_init(value, (float*) values + valueView->stride * (valueAccessor->count - 1));
          } else {
            vec3_init(value, (float*) values + valueView->stride * k);
            vec3_init(next, (float*) values + valueView->stride * (k + 1));
            vec3_lerp(value, next, z);
          }
          vec3_lerp(scale, value, track->alpha);
          touched = true;
          break;
      }
    }
  }

  if (touched) {
    mat4_translate(transform, translation[0], translation[1], translation[2]);
    mat4_rotateQuat(transform, rotation);
    mat4_scale(transform, scale[0], scale[1], scale[2]);
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
