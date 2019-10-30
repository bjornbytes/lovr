#include "thread/thread.h"
#include "thread/channel.h"
#include "core/arr.h"
#include "core/hash.h"
#include "core/map.h"
#include "core/ref.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static struct {
  bool initialized;
  arr_t(Channel*) channels;
  map_t channelMap;
} state;

bool lovrThreadModuleInit() {
  if (state.initialized) return false;
  arr_init(&state.channels);
  map_init(&state.channelMap, 0);
  return state.initialized = true;
}

void lovrThreadModuleDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < state.channels.length; i++) {
    lovrRelease(Channel, state.channels.data[i]);
  }
  arr_free(&state.channels);
  map_free(&state.channelMap);
  state.initialized = false;
}

Channel* lovrThreadGetChannel(const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&state.channelMap, hash);

  if (index == MAP_NIL) {
    index = state.channels.length;
    map_set(&state.channelMap, hash, index);
    arr_push(&state.channels, lovrChannelCreate());
  }

  return state.channels.data[index];
}

Thread* lovrThreadInit(Thread* thread, int (*runner)(void*), Blob* body) {
  lovrRetain(body);
  thread->runner = runner;
  thread->body = body;
  mtx_init(&thread->lock, mtx_plain);
  return thread;
}

void lovrThreadDestroy(void* ref) {
  Thread* thread = ref;
  mtx_destroy(&thread->lock);
  thrd_detach(thread->handle);
  lovrRelease(Blob, thread->body);
  free(thread->error);
}

void lovrThreadStart(Thread* thread) {
  bool running = lovrThreadIsRunning(thread);

  if (running) {
    return;
  }

  free(thread->error);
  thread->error = NULL;
  lovrAssert(thrd_create(&thread->handle, thread->runner, thread) == thrd_success, "Could not create thread...sorry");
}

void lovrThreadWait(Thread* thread) {
  thrd_join(thread->handle, NULL);
}

bool lovrThreadIsRunning(Thread* thread) {
  mtx_lock(&thread->lock);
  bool running = thread->running;
  mtx_unlock(&thread->lock);
  return running;
}

const char* lovrThreadGetError(Thread* thread) {
  return thread->error;
}
