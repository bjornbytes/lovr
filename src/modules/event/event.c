#include "event/event.h"
#include "headset/headset.h"
#include "thread/thread.h"
#include "util.h"
#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static atomic_uint ref;

static struct {
  arr_t(Event) events;
  size_t head;
  mtx_t lock;
} state;

bool lovrEventInit(void) {
  if (!lovrModuleAcquire(&ref)) return true;
  arr_init(&state.events);
  mtx_init(&state.lock, mtx_plain);
  lovrModuleReady(&ref);
  return true;
}

void lovrEventDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  mtx_lock(&state.lock);
  for (size_t i = state.head; i < state.events.length; i++) {
    Event* event = &state.events.data[i];
    switch (event->type) {
      case EVENT_VISIBLE: lovrRelease(event->data.visible.window, lovrWindowDestroy); break;
      case EVENT_FOCUS: lovrRelease(event->data.focus.window, lovrWindowDestroy); break;
      case EVENT_RESIZE: lovrRelease(event->data.resize.window, lovrWindowDestroy); break;
#ifndef LOVR_DISABLE_THREAD
      case EVENT_THREAD_ERROR: lovrRelease(event->data.thread.thread, lovrThreadDestroy); break;
#endif
      case EVENT_CUSTOM:
        for (uint32_t j = 0; j < event->data.custom.count; j++) {
          lovrVariantDestroy(&event->data.custom.data[j]);
        }
        lovrFree(event->data.custom.data);
        break;
      default: break;
    }
  }
  arr_free(&state.events);
  mtx_unlock(&state.lock);
  mtx_destroy(&state.lock);
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

void lovrEventPush(Event event) {
#ifndef LOVR_DISABLE_THREAD
  if (event.type == EVENT_THREAD_ERROR) {
    lovrRetain(event.data.thread.thread);
    event.data.thread.error = lovrStrdup(event.data.thread.error);
  }
#endif

  if (event.type == EVENT_FILECHANGED) {
    event.data.file.path = lovrStrdup(event.data.file.path);
    event.data.file.oldpath = lovrStrdup(event.data.file.oldpath);
  }

  mtx_lock(&state.lock);
  arr_push(&state.events, event);
  mtx_unlock(&state.lock);
}

bool lovrEventPoll(Event* event) {
  mtx_lock(&state.lock);
  if (state.head == state.events.length) {
    state.head = state.events.length = 0;
    mtx_unlock(&state.lock);
    return false;
  }

  *event = state.events.data[state.head++];
  mtx_unlock(&state.lock);
  return true;
}

void lovrEventClear(void) {
  mtx_lock(&state.lock);
  arr_clear(&state.events);
  state.head = 0;
  mtx_unlock(&state.lock);
}
