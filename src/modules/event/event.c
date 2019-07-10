#include "event/event.h"
#include "platform.h"
#include "core/arr.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

static struct {
  bool initialized;
  arr_t(Event, 8) events;
  size_t head;
} state;

void lovrVariantDestroy(Variant* variant) {
  switch (variant->type) {
    case TYPE_STRING: free(variant->value.string); return;
    case TYPE_OBJECT: _lovrRelease(variant->value.object.pointer, variant->value.object.destructor); return;
    default: return;
  }
}

bool lovrEventInit() {
  if (state.initialized) return false;
  arr_init(&state.events);
  return state.initialized = true;
}

void lovrEventDestroy() {
  if (!state.initialized) return;
  arr_free(&state.events);
  memset(&state, 0, sizeof(state));
}

void lovrEventPump() {
  lovrPlatformPollEvents();
}

void lovrEventPush(Event event) {
#ifdef LOVR_ENABLE_THREAD
  if (event.type == EVENT_THREAD_ERROR) {
    lovrRetain(event.data.thread.thread);
  }
#endif

  arr_push(&state.events, event);
}

bool lovrEventPoll(Event* event) {
  if (state.head == state.events.length) {
    state.head = state.events.length = 0;
    return false;
  }

  *event = state.events.data[state.head++];
  return true;
}

void lovrEventClear() {
  arr_clear(&state.events);
  state.head = 0;
}
