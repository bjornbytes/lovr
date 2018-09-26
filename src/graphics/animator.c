#include "graphics/animator.h"
#include <math.h>
#include <stdlib.h>

static Track* lovrAnimatorEnsureTrack(Animator* animator, const char* animation) {
  Track* track = map_get(&animator->trackMap, animation);
  lovrAssert(track, "Animation '%s' does not exist", animation);
  return track;
}

static int trackSortCallback(const void* a, const void* b) {
  return ((Track*) a)->priority < ((Track*) b)->priority;
}

Animator* lovrAnimatorInit(Animator* animator, ModelData* modelData) {
  lovrRetain(modelData);
  animator->modelData = modelData;
  map_init(&animator->trackMap);
  vec_init(&animator->trackList);
  animator->speed = 1;

  /*
  for (int i = 0; i < modelData->animationCount; i++) {
    Animation* animation = &modelData->animations[i];

    Track track = {
      .animation = animation,
      .time = 0,
      .speed = 1,
      .priority = 0,
      .alpha = 1,
      .playing = false,
      .looping = false
    };

    map_set(&animator->trackMap, animation->name, track);
    vec_push(&animator->trackList, map_get(&animator->trackMap, animation->name));
  }
  */

  return animator;
}

void lovrAnimatorDestroy(void* ref) {
  Animator* animator = ref;
  lovrRelease(animator->modelData);
  map_deinit(&animator->trackMap);
  vec_deinit(&animator->trackList);
}

void lovrAnimatorReset(Animator* animator) {
  Track* track; int i;
  vec_foreach(&animator->trackList, track, i) {
    track->time = 0;
    track->speed = 1;
    track->playing = false;
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
      } else if (track->time > track->animation->duration || track->time < 0) {
        track->time = 0;
        track->playing = false;
      }
    }
  }
}

bool lovrAnimatorEvaluate(Animator* animator, const char* bone, mat4 transform) {
  float mixedTranslation[3] = { 0, 0, 0 };
  float mixedRotation[4] = { 0, 0, 0, 1 };
  float mixedScale[3] = { 1, 1, 1 };

  Track* track; int i;
  bool touched = false;
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

      vec3_lerp(mixedTranslation, translation, track->alpha);
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

      quat_slerp(mixedRotation, rotation, track->alpha);
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

      vec3_lerp(mixedScale, scale, track->alpha);
      touched = true;
    }
  }

  if (touched) {
    mat4_translate(transform, mixedTranslation[0], mixedTranslation[1], mixedTranslation[2]);
    mat4_rotateQuat(transform, mixedRotation);
    mat4_scale(transform, mixedScale[0], mixedScale[1], mixedScale[2]);
  }

  return touched;
}

int lovrAnimatorGetAnimationCount(Animator* animator) {
  //oreturn animator->modelData->animationCount;
  return 0;
}

const char* lovrAnimatorGetAnimationName(Animator* animator, int index) {
  if (index < 0 || index >= 0/*animator->modelData->animationCount*/) {
    return NULL;
  }

  //return animator->modelData->animations[index].name;
  return NULL;
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
  float duration = track->animation->duration;

  while (time > duration) {
    time -= duration;
  }

  while (time < 0) {
    time += duration;
  }

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
