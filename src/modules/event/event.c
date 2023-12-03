#include "event/event.h"
#include "thread/thread.h"
#include "util.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static struct {
  uint32_t ref;
  arr_t(Event) events;
  size_t head;
} state;

void lovrVariantDestroy(Variant* variant) {
  switch (variant->type) {
    case TYPE_STRING: lovrFree(variant->value.string.pointer); return;
    case TYPE_OBJECT: lovrRelease(variant->value.object.pointer, variant->value.object.destructor); return;
    case TYPE_MATRIX: lovrFree(variant->value.matrix.data); return;
    default: return;
  }
}

bool lovrEventInit(void) {
  if (atomic_fetch_add(&state.ref, 1)) return false;
  arr_init(&state.events);
  return true;
}

void lovrEventDestroy(void) {
  if (atomic_fetch_sub(&state.ref, 1) != 1) return;
  for (size_t i = state.head; i < state.events.length; i++) {
    Event* event = &state.events.data[i];
    switch (event->type) {
#ifndef LOVR_DISABLE_THREAD
      case EVENT_THREAD_ERROR: lovrRelease(event->data.thread.thread, lovrThreadDestroy); break;
#endif
      case EVENT_CUSTOM:
        for (uint32_t j = 0; j < event->data.custom.count; j++) {
          lovrVariantDestroy(&event->data.custom.data[j]);
        }
        break;
      default: break;
    }
  }
  arr_free(&state.events);
  memset(&state, 0, sizeof(state));
}

void lovrEventPush(Event event) {
#ifndef LOVR_DISABLE_THREAD
  if (event.type == EVENT_THREAD_ERROR) {
    lovrRetain(event.data.thread.thread);
    size_t length = strlen(event.data.thread.error);
    char* copy = malloc(length + 1);
    memcpy(copy, event.data.thread.error, length + 1);
    event.data.thread.error = copy;
  }
#endif

  if (event.type == EVENT_FILECHANGED) {
    size_t length = strlen(event.data.file.path);
    char* copy = malloc(length + 1);
    memcpy(copy, event.data.file.path, length + 1);
    event.data.file.path = copy;

    if (event.data.file.oldpath) {
      length = strlen(event.data.file.oldpath);
      copy = malloc(length + 1);
      memcpy(copy, event.data.file.oldpath, length + 1);
      event.data.file.oldpath = copy;
    }
  }

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

void lovrEventClear(void) {
  arr_clear(&state.events);
  state.head = 0;
}
