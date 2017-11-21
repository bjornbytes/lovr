#include "graphics/animator.h"
#include "math/vec3.h"
#include "math/quat.h"
#include <math.h>

static Track* lovrAnimatorEnsureTrack(Animator* animator, const char* animation) {
  Track* track = map_get(&animator->trackMap, animation);
  lovrAssert(track, "Animation '%s' does not exist", animation);
  return track;
}

static int trackSortCallback(const void* a, const void* b) {
  return ((Track*) a)->priority < ((Track*) b)->priority;
}

Animator* lovrAnimatorCreate(AnimationData* animationData) {
  Animator* animator = lovrAlloc(sizeof(Animator), lovrAnimatorDestroy);
  if (!animator) return NULL;

  animator->animationData = animationData;
  map_init(&animator->trackMap);
  vec_init(&animator->trackList);
  animator->speed = 1;

  for (int i = 0; i < animationData->animations.length; i++) {
    Animation* animation = &animationData->animations.data[i];

    Track track = {
      .animation = animation,
      .time = 0,
      .speed = 1,
      .priority = 0,
      .playing = false,
      .looping = false
    };

    map_set(&animator->trackMap, animation->name, track);
    vec_push(&animator->trackList, map_get(&animator->trackMap, animation->name));
  }

  return animator;
}

void lovrAnimatorDestroy(const Ref* ref) {
  Animator* animator = containerof(ref, Animator);
  map_deinit(&animator->trackMap);
  vec_deinit(&animator->trackList);
  free(animator);
}

void lovrAnimatorReset(Animator* animator) {
  Track* track; int i;
  vec_foreach(&animator->trackList, track, i) {
    track->time = 0;
    track->speed = 1;
    track->playing = true;
    track->looping = false;
  }
  animator->speed = 1;
}

void lovrAnimatorUpdate(Animator* animator, float dt) {
  Track* track; int i;
  vec_foreach(&animator->trackList, track, i) {
    if (track->playing) {
      track->time += dt * track->speed * animator->speed;

      if (track->looping) {
        track->time = fmodf(track->time, track->animation->duration);
      } else if (track->time > track->animation->duration) {
        track->time = 0;
        track->playing = false;
      }
    }
  }
}

bool lovrAnimatorEvaluate(Animator* animator, const char* bone, mat4 transform) {
  bool touched = false;
  Track* track; int i;
  vec_foreach(&animator->trackList, track, i) {
    Animation* animation = track->animation;
    AnimationChannel* channel = map_get(&animation->channels, bone);

    if (!channel || !track->playing) {
      continue;
    }

    float time = fmodf(track->time, animation->duration);
    int i;

    // Position
    if (channel->positionKeyframes.length > 0) {
      i = 0;
      while (i < channel->positionKeyframes.length) {
        if (channel->positionKeyframes.data[i].time >= time) {
          break;
        } else {
          i++;
        }
      }

      float translation[3];
      if (i == 0) {
        Keyframe keyframe = channel->positionKeyframes.data[0];
        vec3_init(translation, keyframe.data);
      } else if (i >= channel->positionKeyframes.length) {
        Keyframe keyframe = channel->positionKeyframes.data[channel->positionKeyframes.length - 1];
        vec3_init(translation, keyframe.data);
      } else {
        Keyframe before, after;
        before = channel->positionKeyframes.data[i - 1];
        after = channel->positionKeyframes.data[i];
        float t = (time - before.time) / (after.time - before.time);
        vec3_lerp(vec3_init(translation, before.data), after.data, t);
      }

      mat4_translate(transform, translation[0], translation[1], translation[2]);
      touched = true;
    }

    // Rotation
    if (channel->rotationKeyframes.length > 0) {
      i = 0;
      while (i < channel->rotationKeyframes.length) {
        if (channel->rotationKeyframes.data[i].time >= time) {
          break;
        } else {
          i++;
        }
      }

      float rotation[4];
      if (i == 0) {
        Keyframe keyframe = channel->rotationKeyframes.data[0];
        quat_init(rotation, keyframe.data);
      } else if (i >= channel->rotationKeyframes.length) {
        Keyframe keyframe = channel->rotationKeyframes.data[channel->rotationKeyframes.length - 1];
        quat_init(rotation, keyframe.data);
      } else {
        Keyframe before, after;
        before = channel->rotationKeyframes.data[i - 1];
        after = channel->rotationKeyframes.data[i];
        float t = (time - before.time) / (after.time - before.time);
        quat_slerp(quat_init(rotation, before.data), after.data, t);
      }

      mat4_rotateQuat(transform, rotation);
      touched = true;
    }

    // Scale
    if (channel->scaleKeyframes.length > 0) {
      i = 0;
      while (i < channel->scaleKeyframes.length) {
        if (channel->scaleKeyframes.data[i].time >= time) {
          break;
        } else {
          i++;
        }
      }

      float scale[3];
      if (i == 0) {
        Keyframe keyframe = channel->scaleKeyframes.data[0];
        vec3_init(scale, keyframe.data);
      } else if (i >= channel->scaleKeyframes.length) {
        Keyframe keyframe = channel->scaleKeyframes.data[channel->scaleKeyframes.length - 1];
        vec3_init(scale, keyframe.data);
      } else {
        Keyframe before, after;
        before = channel->scaleKeyframes.data[i - 1];
        after = channel->scaleKeyframes.data[i];
        float t = (time - before.time) / (after.time - before.time);
        vec3_lerp(vec3_init(scale, before.data), after.data, t);
      }

      mat4_scale(transform, scale[0], scale[1], scale[2]);
      touched = true;
    }
  }

  return touched;
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

int lovrAnimatorGetPriority(Animator* animator, const char* animation) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  return track->priority;
}

void lovrAnimatorSetPriority(Animator* animator, const char* animation, int priority) {
  Track* track = lovrAnimatorEnsureTrack(animator, animation);
  track->priority = priority;
  vec_sort(&animator->trackList, trackSortCallback);
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
