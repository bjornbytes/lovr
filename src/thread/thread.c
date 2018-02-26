#include "thread/thread.h"
#include "luax.h"
#include "api.h"
#include <string.h>

static ThreadState state;

void lovrThreadInit() {
  if (state.initialized) return;
  map_init(&state.channels);
  state.initialized = true;
}

void lovrThreadDeinit() {
  if (!state.initialized) return;
  map_deinit(&state.channels);
  state.initialized = false;
}

Channel* lovrThreadGetChannel(const char* name) {
  Channel** channel = (Channel**) map_get(&state.channels, name);

  if (channel) {
    return *channel;
  } else {
    Channel* channel = lovrChannelCreate();
    map_set(&state.channels, name, channel);
    return channel;
  }
}

Thread* lovrThreadCreate(int (*runner)(void*), const char* body) {
  Thread* thread = lovrAlloc(sizeof(Thread), lovrThreadDestroy);
  if (!thread) return NULL;

  thread->runner = runner;
  thread->body = body;
  thread->error = NULL;
  thread->running = false;
  mtx_init(&thread->lock, mtx_plain);

  return thread;
}

void lovrThreadDestroy(void* ref) {
  Thread* thread = ref;
  mtx_destroy(&thread->lock);
  thrd_detach(thread->handle);
  free(thread);
}

void lovrThreadStart(Thread* thread) {
  bool running = lovrThreadIsRunning(thread);

  if (running) {
    return;
  }

  if (thrd_create(&thread->handle, thread->runner, thread) != thrd_success) {
    lovrThrow("Could not create thread...sorry");
    return;
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
