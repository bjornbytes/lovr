#include "loaders/animation.h"
#include <stdlib.h>

AnimationData* lovrAnimationDataCreate() {
  AnimationData* animationData = malloc(sizeof(AnimationData));
  if (!animationData) return NULL;

  vec_init(&animationData->animations);

  return animationData;
}

void lovrAnimationDataDestroy(AnimationData* animationData) {
  vec_deinit(&animationData->animations);
  free(animationData);
}
