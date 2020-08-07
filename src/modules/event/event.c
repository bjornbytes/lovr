#include "event/event.h"
#include "thread/thread.h"
#include "core/arr.h"
#include "core/os.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

static struct {
  bool initialized;
  arr_t(Event) events;
  size_t head;
} state;

static void onKeyboardEvent(ButtonAction action, KeyCode key, uint32_t scancode, bool repeat) {
  lovrEventPush((Event) {
    .type = action == BUTTON_PRESSED ? EVENT_KEYPRESSED : EVENT_KEYRELEASED,
    .data.key.code = key,
    .data.key.scancode = scancode,
    .data.key.repeat = repeat
  });
}

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
  lovrPlatformOnKeyboardEvent(onKeyboardEvent);
  return state.initialized = true;
}

void lovrEventDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < state.events.length; i++) {
    Event* event = &state.events.data[i];
    switch (event->type) {
      case EVENT_THREAD_ERROR: lovrRelease(Thread, event->data.thread.thread); break;
      case EVENT_CUSTOM:
        for (uint32_t j = 0; j < event->data.custom.count; j++) {
          lovrVariantDestroy(&event->data.custom.data[j]);
        }
        break;
      default: break;
    }
  }
  arr_free(&state.events);
  lovrPlatformOnKeyboardEvent(NULL);
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
