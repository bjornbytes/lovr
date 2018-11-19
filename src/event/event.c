#include "event/event.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

static EventState state;

bool lovrEventInit() {
  if (state.initialized) return false;
  vec_init(&state.pumps);
  vec_init(&state.events);
  lovrEventAddPump(lovrPlatformPollEvents);
  atexit(lovrEventDestroy);
  return state.initialized = true;
}

void lovrEventDestroy() {
  if (!state.initialized) return;
  vec_deinit(&state.pumps);
  vec_deinit(&state.events);
  memset(&state, 0, sizeof(EventState));
}

void lovrEventAddPump(EventPump pump) {
  vec_push(&state.pumps, pump);
}

void lovrEventRemovePump(EventPump pump) {
  vec_remove(&state.pumps, pump);
}

void lovrEventPump() {
  int i; EventPump pump;
  vec_foreach(&state.pumps, pump, i) {
    pump();
  }
}

void lovrEventPush(Event event) {
  vec_insert(&state.events, 0, event);
}

bool lovrEventPoll(Event* event) {
  if (state.events.length == 0) {
    return false;
  }

  *event = vec_pop(&state.events);
  return true;
}

void lovrEventClear() {
  vec_clear(&state.events);
}
