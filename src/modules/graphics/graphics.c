#include "graphics/graphics.h"
#include <string.h>

static struct {
  bool initialized;
} state;

bool lovrGraphicsInit() {
  if (state.initialized) return false;
  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(state));
}
