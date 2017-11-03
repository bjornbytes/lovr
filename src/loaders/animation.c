#include "loaders/animation.h"
#include <stdlib.h>

AnimationData* lovrAnimationDataCreate(const char* name) {
  AnimationData* animationData = malloc(sizeof(AnimationData));
  if (!animationData) return NULL;

  vec_init(&animationData->animations);

  return animationData;
}

void lovrAnimationDataDestroy(AnimationData* animationData) {
  vec_deinit(&animationData->animations);
  free(animationData);
}
