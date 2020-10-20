#include "graphics/graphics.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/util.h"
#include <string.h>

#define MAX_TRANSFORMS 64

static struct {
  bool initialized;
  float transforms[MAX_TRANSFORMS][16];
  uint32_t transform;
} state;

void onMessage(void* context, const char* message, int severe) {
  lovrLog(severe ? LOG_ERROR : LOG_DEBUG, "GPU", message);
}

bool lovrGraphicsInit(bool debug) {
  if (state.initialized) return false;

  gpu_config config = {
    .debug = debug,
    .callback = onMessage
  };

  if (!gpu_init(&config)) {
    return false;
  }

  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  gpu_destroy();
  memset(&state, 0, sizeof(state));
}

// Transforms

void lovrGraphicsPush() {
  lovrAssert(++state.transform < MAX_TRANSFORMS, "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(state.transforms[state.transform], state.transforms[state.transform - 1]);
}

void lovrGraphicsPop() {
  lovrAssert(--state.transform >= 0, "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrGraphicsOrigin() {
  mat4_identity(state.transforms[state.transform]);
}

void lovrGraphicsTranslate(vec3 translation) {
  mat4_translate(state.transforms[state.transform], translation[0], translation[1], translation[2]);
}

void lovrGraphicsRotate(quat rotation) {
  mat4_rotateQuat(state.transforms[state.transform], rotation);
}

void lovrGraphicsScale(vec3 scale) {
  mat4_scale(state.transforms[state.transform], scale[0], scale[1], scale[2]);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_mul(state.transforms[state.transform], transform);
}
