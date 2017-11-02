#include "loaders/animation.h"
#include <stdlib.h>

AnimationData* lovrAnimationDataCreate(const char* name) {
  AnimationData* animationData = malloc(sizeof(AnimationData));
  if (!animationData) return NULL;

  animationData->name = name;
  map_init(&animationData->channels);

  return animationData;
}

void lovrAnimationDataDestroy(AnimationData* animationData) {
  map_deinit(&animationData->channels);
  free(animationData);
}
