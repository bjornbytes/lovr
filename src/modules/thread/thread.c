#include "thread/thread.h"
#include "thread/channel.h"
#include "core/arr.h"
#include "core/hash.h"
#include "core/map.h"
#include "core/ref.h"
#include "core/util.h"
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
    arr_push(&state.channels, lovrChannelCreate(hash));
  }

  return state.channels.data[index];
}

void lovrThreadRemoveChannel(uint64_t hash) {
  uint64_t index = map_get(&state.channelMap, hash);

  if (index == MAP_NIL) {
    return;
  }

  map_remove(&state.channelMap, hash);
  arr_splice(&state.channels, index, 1);
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

void lovrThreadStart(Thread* thread, Variant* arguments, size_t argumentCount) {
  bool running = lovrThreadIsRunning(thread);

  if (running) {
    return;
  }

  free(thread->error);
  thread->error = NULL;
  lovrAssert(argumentCount <= MAX_THREAD_ARGUMENTS, "Too many Thread arguments (max is %d)\n", MAX_THREAD_ARGUMENTS);
  thread->argumentCount = argumentCount;
  memcpy(thread->arguments, arguments, argumentCount * sizeof(Variant));
  if (thrd_create(&thread->handle, thread->runner, thread) != thrd_success) {
    lovrThrow("Could not create thread...sorry");
  }
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
