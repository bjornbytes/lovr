#include "event/event.h"
#include "lib/glfw.h"
#include <stdlib.h>

static EventState state;

void lovrEventInit() {
  if (state.initialized) return;
  vec_init(&state.pumps);
  vec_init(&state.events);
  mtx_init(&state.lock, mtx_plain);
  lovrEventAddPump(glfwPollEvents);
  atexit(lovrEventDestroy);
  state.initialized = true;
}

void lovrEventDestroy() {
  if (!state.initialized) return;
  vec_deinit(&state.pumps);
  vec_deinit(&state.events);
  mtx_destroy(&state.lock);
  memset(&state, 0, sizeof(EventState));
}

void lovrEventAddPump(EventPump pump) {
  mtx_lock(&state.lock);
  vec_push(&state.pumps, pump);
  mtx_unlock(&state.lock);
}

void lovrEventRemovePump(EventPump pump) {
  mtx_lock(&state.lock);
  vec_remove(&state.pumps, pump);
  mtx_unlock(&state.lock);
}

void lovrEventPump() {
  int i; EventPump pump;
  vec_foreach(&state.pumps, pump, i) {
    pump();
  }
}

void lovrEventPush(Event event) {
  mtx_lock(&state.lock);
  vec_insert(&state.events, 0, event);
  mtx_unlock(&state.lock);
}

bool lovrEventPoll(Event* event) {
  mtx_lock(&state.lock);
  if (state.events.length == 0) {
    mtx_unlock(&state.lock);
    return false;
  }

  *event = vec_pop(&state.events);
  mtx_unlock(&state.lock);
  return true;
}

void lovrEventClear() {
  mtx_lock(&state.lock);
  vec_clear(&state.events);
  mtx_unlock(&state.lock);
}
