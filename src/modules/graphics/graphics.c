#include "graphics/graphics.h"
#include "core/gpu.h"
#include "util.h"
#include <string.h>

static struct {
  bool initialized;
} state;

// Helpers

static void onMessage(void* context, const char* message, bool severe);

// Entry

bool lovrGraphicsInit(bool debug) {
  if (state.initialized) return false;

  gpu_config config = {
    .debug = debug,
    .callback = onMessage
  };

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  gpu_destroy();
  memset(&state, 0, sizeof(state));
}

// Helpers

static void onMessage(void* context, const char* message, bool severe) {
  if (severe) {
    lovrLog(LOG_ERROR, "GPU", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
