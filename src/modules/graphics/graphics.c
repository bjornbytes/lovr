#include "graphics/graphics.h"
#include <string.h>

static struct {
  bool initialized;
} state;

bool lovrGraphicsInit() {
  if (state.initialized) return false;
  return true;
}

void lovrGraphicsDestroy() {
  memset(&state, 0, sizeof(state));
}
