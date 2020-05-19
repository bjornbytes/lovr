#include "thread/thread.h"
#include "thread/channel.h"
#include "core/arr.h"
#include "core/map.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

typedef union {
  uint64_t u64;
  Channel* channel;
} ChannelEntry;

static struct {
  bool initialized;
  mtx_t channelLock;
  map_t channels;
} state;

bool lovrThreadModuleInit() {
  if (state.initialized) return false;
  mtx_init(&state.channelLock, mtx_plain);
  map_init(&state.channels, 0);
  return state.initialized = true;
}

void lovrThreadModuleDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < state.channels.size; i++) {
    if (state.channels.values[i] != MAP_NIL) {
      ChannelEntry entry = { state.channels.values[i] };
      lovrRelease(Channel, entry.channel);
    }
  }
  mtx_destroy(&state.channelLock);
  map_free(&state.channels);
  state.initialized = false;
}

Channel* lovrThreadGetChannel(const char* name) {
  uint64_t hash = hash64(name, strlen(name));

  mtx_lock(&state.channelLock);
  ChannelEntry entry = { map_get(&state.channels, hash) };

  if (entry.u64 == MAP_NIL) {
    entry.channel = lovrChannelCreate(hash);
    map_set(&state.channels, hash, entry.u64);
  }

  mtx_unlock(&state.channelLock);
  return entry.channel;
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
