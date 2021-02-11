#include "thread/channel.h"
#include "event/event.h"
#include "core/util.h"
#include "lib/tinycthread/tinycthread.h"
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

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

Channel* lovrChannelCreate(uint64_t hash) {
  Channel* channel = calloc(1, sizeof(Channel));
  lovrAssert(channel, "Out of memory");
  channel->ref = 1;
  arr_init(&channel->messages, realloc);
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
