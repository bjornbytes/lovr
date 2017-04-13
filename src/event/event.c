#include "event/event.h"
#include "lib/glfw.h"
#include <stdlib.h>

static EventState state;

void lovrEventInit() {
  vec_init(&state.pumps);
  vec_init(&state.events);
  lovrEventAddPump(glfwPollEvents);
  atexit(lovrEventDestroy);
}

void lovrEventDestroy() {
  vec_deinit(&state.pumps);
  vec_deinit(&state.events);
}

void lovrEventAddPump(EventPump pump) {
  vec_push(&state.pumps, pump);
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

Event* lovrEventPoll() {
  if (state.events.length == 0) {
    return NULL;
  }

  return &vec_pop(&state.events);
}

void lovrEventClear() {
  vec_clear(&state.events);
}
