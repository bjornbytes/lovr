#include "thread/thread.h"
#include "data/blob.h"
#include "event/event.h"
#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct Thread {
  uint32_t ref;
  thrd_t handle;
  mtx_t lock;
  ThreadFunction* function;
  Blob* body;
  Variant arguments[MAX_THREAD_ARGUMENTS];
  uint32_t argumentCount;
  char* error;
  bool running;
};

struct Channel {
  uint32_t ref;
  mtx_t lock;
  cnd_t cond;
  arr_t(Variant) messages;
  size_t head;
  uint64_t sent;
  uint64_t received;
  uint64_t hash;
};

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
      lovrRelease((Channel*) (uintptr_t) state.channels.values[i], lovrChannelDestroy);
    }
  }
  mtx_destroy(&state.channelLock);
  map_free(&state.channels);
  state.initialized = false;
}

Channel* lovrThreadGetChannel(const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  Channel* channel = NULL;

  mtx_lock(&state.channelLock);
  uint64_t entry = map_get(&state.channels, hash);

  if (entry == MAP_NIL) {
    channel = lovrChannelCreate(hash);
    map_set(&state.channels, hash, (uint64_t) (uintptr_t) channel);
  } else {
    channel = (Channel*) (uintptr_t) entry;
  }

  mtx_unlock(&state.channelLock);
  return channel;
}

// Thread

static int threadFunction(void* data) {
  Thread* thread = data;

  char* error = thread->function(thread, thread->body, thread->arguments, thread->argumentCount);

  mtx_lock(&thread->lock);
  thread->running = false;
  if (error) {
    thread->error = error;
    lovrEventPush((Event) {
      .type = EVENT_THREAD_ERROR,
      .data.thread = { thread, thread->error }
    });
  }
  mtx_unlock(&thread->lock);

  lovrRelease(thread, lovrThreadDestroy);
  return 0;
}

Thread* lovrThreadCreate(ThreadFunction* function, Blob* body) {
  Thread* thread = calloc(1, sizeof(Thread));
  lovrAssert(thread, "Out of memory");
  thread->ref = 1;
  thread->body = body;
  thread->function = function;
  mtx_init(&thread->lock, mtx_plain);
  lovrRetain(body);
  return thread;
}

void lovrThreadDestroy(void* ref) {
  Thread* thread = ref;
  mtx_destroy(&thread->lock);
  if (thread->handle) thrd_detach(thread->handle);
  lovrRelease(thread->body, lovrBlobDestroy);
  free(thread->error);
  free(thread);
}

void lovrThreadStart(Thread* thread, Variant* arguments, uint32_t argumentCount) {
  mtx_lock(&thread->lock);
  if (thread->running) {
    mtx_unlock(&thread->lock);
    return;
  }

  free(thread->error);
  thread->error = NULL;

  lovrAssert(argumentCount <= MAX_THREAD_ARGUMENTS, "Too many Thread arguments (max is %d)", MAX_THREAD_ARGUMENTS);
  memcpy(thread->arguments, arguments, argumentCount * sizeof(Variant));
  thread->argumentCount = argumentCount;

  lovrRetain(thread);

  if (thrd_create(&thread->handle, threadFunction, thread) != thrd_success) {
    mtx_unlock(&thread->lock);
    lovrRelease(thread, lovrThreadDestroy);
    lovrThrow("Could not create thread...sorry");
  }

  thread->running = true;
  mtx_unlock(&thread->lock);
}

void lovrThreadWait(Thread* thread) {
  if (thread->handle) thrd_join(thread->handle, NULL);
}

bool lovrThreadIsRunning(Thread* thread) {
  return thread->running;
}

const char* lovrThreadGetError(Thread* thread) {
  return thread->error;
}

// Channel

Channel* lovrChannelCreate(uint64_t hash) {
  Channel* channel = calloc(1, sizeof(Channel));
  lovrAssert(channel, "Out of memory");
  channel->ref = 1;
  arr_init(&channel->messages, arr_alloc);
  mtx_init(&channel->lock, mtx_plain | mtx_timed);
  cnd_init(&channel->cond);
  channel->hash = hash;
  return channel;
}

void lovrChannelDestroy(void* ref) {
  Channel* channel = ref;
  lovrChannelClear(channel);
  arr_free(&channel->messages);
  mtx_destroy(&channel->lock);
  cnd_destroy(&channel->cond);
  free(channel);
}

bool lovrChannelPush(Channel* channel, Variant* variant, double timeout, uint64_t* id) {
  mtx_lock(&channel->lock);
  if (channel->messages.length == 0) {
    lovrRetain(channel);
  }
  arr_push(&channel->messages, *variant);
  *id = ++channel->sent;
  cnd_broadcast(&channel->cond);

  if (isnan(timeout) || timeout < 0) {
    mtx_unlock(&channel->lock);
    return false;
  }

  while (channel->received < *id && timeout >= 0) {
    if (isinf(timeout)) {
      cnd_wait(&channel->cond, &channel->lock);
    } else {
      struct timespec start;
      struct timespec until;
      struct timespec stop;
      timespec_get(&start, TIME_UTC);
      double whole, fraction;
      fraction = modf(timeout, &whole);
      until.tv_sec = start.tv_sec + whole;
      until.tv_nsec = start.tv_nsec + fraction * 1e9;
      cnd_timedwait(&channel->cond, &channel->lock, &until);
      timespec_get(&stop, TIME_UTC);
      timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / 1e9;
    }
  }

  bool read = channel->received >= *id;
  mtx_unlock(&channel->lock);
  return read;
}

bool lovrChannelPop(Channel* channel, Variant* variant, double timeout) {
  mtx_lock(&channel->lock);

  do {
    if (channel->head < channel->messages.length) {
      *variant = channel->messages.data[channel->head++];
      if (channel->head == channel->messages.length) {
        channel->head = channel->messages.length = 0;
        lovrRelease(channel, lovrChannelDestroy);
      }
      channel->received++;
      cnd_broadcast(&channel->cond);
      mtx_unlock(&channel->lock);
      return true;
    } else if (isnan(timeout) || timeout < 0) {
      mtx_unlock(&channel->lock);
      return false;
    }

    if (isinf(timeout)) {
      cnd_wait(&channel->cond, &channel->lock);
    } else {
      struct timespec start;
      struct timespec until;
      struct timespec stop;
      timespec_get(&start, TIME_UTC);
      double whole, fraction;
      fraction = modf(timeout, &whole);
      until.tv_sec = start.tv_sec + whole;
      until.tv_nsec = start.tv_nsec + fraction * 1e9;
      cnd_timedwait(&channel->cond, &channel->lock, &until);
      timespec_get(&stop, TIME_UTC);
      timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double) 1e9;
    }
  } while (1);
}

bool lovrChannelPeek(Channel* channel, Variant* variant) {
  mtx_lock(&channel->lock);

  if (channel->head < channel->messages.length) {
    *variant = channel->messages.data[channel->head];
    mtx_unlock(&channel->lock);
    return true;
  }

  mtx_unlock(&channel->lock);
  return false;
}

void lovrChannelClear(Channel* channel) {
  mtx_lock(&channel->lock);
  for (size_t i = channel->head; i < channel->messages.length; i++) {
    lovrVariantDestroy(&channel->messages.data[i]);
  }
  channel->received = channel->sent;
  arr_clear(&channel->messages);
  channel->head = 0;
  cnd_broadcast(&channel->cond);
  mtx_unlock(&channel->lock);
}

uint64_t lovrChannelGetCount(Channel* channel) {
  mtx_lock(&channel->lock);
  uint64_t length = channel->messages.length - channel->head;
  mtx_unlock(&channel->lock);
  return length;
}

bool lovrChannelHasRead(Channel* channel, uint64_t id) {
  mtx_lock(&channel->lock);
  bool received = channel->received >= id;
  mtx_unlock(&channel->lock);
  return received;
}
