#include "thread/thread.h"
#include "data/blob.h"
#include "event/event.h"
#include "core/job.h"
#include "core/os.h"
#include "util.h"
#include "core/threads.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct Thread {
  lovr_atomic_uint ref;
  lovr_thread handle;
  lovr_mutex lock;
  lovr_mutex waitLock;
  ThreadFunction* function;
  Blob* body;
  Variant arguments[MAX_THREAD_ARGUMENTS];
  uint32_t argumentCount;
  char* error;
  bool joinable;
  bool running;
};

struct Channel {
  lovr_atomic_uint ref;
  lovr_mutex lock;
  lovr_cond cond;
  arr_t(Variant) messages;
  size_t head;
  uint64_t sent;
  uint64_t received;
  uint64_t hash;
};

static lovr_atomic_uint ref;

static struct {
  uint32_t workers;
  void (*onWorkerQuit)(void);
  lovr_mutex channelLock;
  map_t channels;
} state;

static void workerInit(uint32_t id) {
  lovrProfileSetThreadName("Worker");
  os_thread_set_name("Worker");
}

static void workerQuit(uint32_t id) {
  if (state.onWorkerQuit) {
    state.onWorkerQuit();
  }
}

bool lovrThreadModuleInit(int32_t workers, void (*onWorkerQuit)(void)) {
  if (!lovrModuleAcquire(&ref)) return true;

  lovr_mutex_create(&state.channelLock);
  map_init(&state.channels, 0);

  uint32_t cores = os_get_core_count();
  if (workers < 0) workers += cores;
  state.workers = MAX(workers, 0);
  state.onWorkerQuit = onWorkerQuit;
  job_init(state.workers, workerInit, workerQuit);

  lovrModuleReady(&ref);
  return true;
}

void lovrThreadModuleDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  for (size_t i = 0; i < state.channels.size; i++) {
    if (state.channels.values[i] != MAP_NIL) {
      lovrRelease((Channel*) (uintptr_t) state.channels.values[i], lovrChannelDestroy);
    }
  }
  lovr_mutex_destroy(&state.channelLock);
  map_free(&state.channels);
  job_destroy();
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

uint32_t lovrThreadGetWorkerCount(void) {
  return state.workers;
}

Channel* lovrThreadGetChannel(const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  Channel* channel = NULL;

  lovr_mutex_lock(&state.channelLock);
  uint64_t entry = map_get(&state.channels, hash);

  if (entry == MAP_NIL) {
    channel = lovrChannelCreate(hash);
    map_set(&state.channels, hash, (uint64_t) (uintptr_t) channel);
  } else {
    channel = (Channel*) (uintptr_t) entry;
  }

  lovr_mutex_unlock(&state.channelLock);
  return channel;
}

// Thread

static int threadFunction(void* data) {
  Thread* thread = data;

  os_thread_attach();

  char* error = thread->function(thread, thread->body, thread->arguments, thread->argumentCount);

  os_thread_detach();

  lovr_mutex_lock(&thread->lock);
  thread->running = false;
  if (error) {
    thread->error = error;
    lovrEventPush((Event) {
      .type = EVENT_THREAD_ERROR,
      .data.thread = { thread, thread->error }
    });
  }
  lovr_mutex_unlock(&thread->lock);

  lovrRelease(thread, lovrThreadDestroy);
  return 0;
}

Thread* lovrThreadCreate(ThreadFunction* function, Blob* body) {
  Thread* thread = lovrCalloc(sizeof(Thread));
lovr_atomic_store(&thread->ref, 1);
  thread->body = body;
  thread->function = function;
  lovr_mutex_create(&thread->lock);
  lovr_mutex_create(&thread->waitLock);
  lovrRetain(body);
  return thread;
}

void lovrThreadDestroy(void* ref) {
  Thread* thread = ref;
  lovr_mutex_destroy(&thread->lock);
  lovr_mutex_destroy(&thread->waitLock);
  if (thread->joinable) lovr_thread_detach(thread->handle);
  for (uint32_t i = 0; i < thread->argumentCount; i++) {
    lovrVariantDestroy(&thread->arguments[i]);
  }
  lovrRelease(thread->body, lovrBlobDestroy);
  lovrFree(thread->error);
  lovrFree(thread);
}

bool lovrThreadStart(Thread* thread, Variant* arguments, uint32_t argumentCount) {
  lovrCheck(argumentCount <= MAX_THREAD_ARGUMENTS, "Too many Thread arguments (max is %d)", MAX_THREAD_ARGUMENTS);

  lovr_mutex_lock(&thread->lock);
  if (thread->running) {
    lovr_mutex_unlock(&thread->lock);
    return true;
  }

  lovrFree(thread->error);
  thread->error = NULL;

  for (uint32_t i = 0; i < thread->argumentCount; i++) {
    lovrVariantDestroy(&thread->arguments[i]);
  }

  memcpy(thread->arguments, arguments, argumentCount * sizeof(Variant));
  thread->argumentCount = argumentCount;

  lovrRetain(thread);

  if (!lovr_thread_create(&thread->handle, threadFunction, "lovr", thread)) {
    lovr_mutex_unlock(&thread->lock);
    lovrRelease(thread, lovrThreadDestroy);
    return lovrSetError("Could not create thread...sorry");
  }

  thread->joinable = true;
  thread->running = true;
  lovr_mutex_unlock(&thread->lock);
  return true;
}

void lovrThreadWait(Thread* thread) {
  lovr_mutex_lock(&thread->waitLock);

  if (thread->joinable) {
    lovr_thread_join(thread->handle);
    thread->joinable = false;
  }

  lovr_mutex_unlock(&thread->waitLock);
}

bool lovrThreadIsRunning(Thread* thread) {
  return thread->running;
}

const char* lovrThreadGetError(Thread* thread) {
  return thread->error;
}

// Channel

Channel* lovrChannelCreate(uint64_t hash) {
  Channel* channel = lovrCalloc(sizeof(Channel));
lovr_atomic_store(&channel->ref, 1);
  arr_init(&channel->messages);
  lovr_mutex_create(&channel->lock);
  lovr_cond_create(&channel->cond);
  channel->hash = hash;
  return channel;
}

void lovrChannelDestroy(void* ref) {
  Channel* channel = ref;
  lovrChannelClear(channel);
  arr_free(&channel->messages);
  lovr_mutex_destroy(&channel->lock);
  lovr_cond_destroy(&channel->cond);
  lovrFree(channel);
}

bool lovrChannelPush(Channel* channel, Variant* variant, double timeout, uint64_t* id) {
  lovr_mutex_lock(&channel->lock);
  if (channel->messages.length == 0) {
    lovrRetain(channel);
  }
  arr_push(&channel->messages, *variant);
  *id = ++channel->sent;
  lovr_cond_broadcast(&channel->cond);

  if (isnan(timeout) || timeout < 0) {
    lovr_mutex_unlock(&channel->lock);
    return false;
  }

  while (channel->received < *id && timeout >= 0) {
    if (isinf(timeout)) {
      lovr_cond_wait(&channel->cond, &channel->lock);
    } else {
      lovr_cond_timedwait(&channel->cond, &channel->lock, &timeout);
    }
  }

  bool read = channel->received >= *id;
  lovr_mutex_unlock(&channel->lock);
  return read;
}

bool lovrChannelPop(Channel* channel, Variant* variant, double timeout) {
  lovr_mutex_lock(&channel->lock);

  do {
    if (channel->head < channel->messages.length) {
      *variant = channel->messages.data[channel->head++];
      if (channel->head == channel->messages.length) {
        channel->head = channel->messages.length = 0;
        lovrRelease(channel, lovrChannelDestroy);
      }
      channel->received++;
      lovr_cond_broadcast(&channel->cond);
      lovr_mutex_unlock(&channel->lock);
      return true;
    } else if (isnan(timeout) || timeout < 0) {
      lovr_mutex_unlock(&channel->lock);
      return false;
    }

    if (isinf(timeout)) {
      lovr_cond_wait(&channel->cond, &channel->lock);
    } else {
      lovr_cond_timedwait(&channel->cond, &channel->lock, &timeout);
    }
  } while (1);
}

bool lovrChannelPeek(Channel* channel, Variant* variant) {
  lovr_mutex_lock(&channel->lock);

  if (channel->head < channel->messages.length) {
    *variant = channel->messages.data[channel->head];
    lovr_mutex_unlock(&channel->lock);
    return true;
  }

  lovr_mutex_unlock(&channel->lock);
  return false;
}

void lovrChannelClear(Channel* channel) {
  lovr_mutex_lock(&channel->lock);
  for (size_t i = channel->head; i < channel->messages.length; i++) {
    lovrVariantDestroy(&channel->messages.data[i]);
  }
  channel->received = channel->sent;
  arr_clear(&channel->messages);
  channel->head = 0;
  lovr_cond_broadcast(&channel->cond);
  lovr_mutex_unlock(&channel->lock);
}

uint64_t lovrChannelGetCount(Channel* channel) {
  lovr_mutex_lock(&channel->lock);
  uint64_t length = channel->messages.length - channel->head;
  lovr_mutex_unlock(&channel->lock);
  return length;
}

bool lovrChannelHasRead(Channel* channel, uint64_t id) {
  lovr_mutex_lock(&channel->lock);
  bool received = channel->received >= id;
  lovr_mutex_unlock(&channel->lock);
  return received;
}
