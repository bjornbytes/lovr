#include "thread/channel.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

Channel* lovrChannelCreate() {
  Channel* channel = lovrAlloc(sizeof(Channel), lovrChannelDestroy);
  if (!channel) return NULL;

  vec_init(&channel->messages);
  mtx_init(&channel->lock, mtx_plain | mtx_timed);
  cnd_init(&channel->cond);
  channel->sent = 0;
  channel->received = 0;

  return channel;
}

void lovrChannelDestroy(void* ref) {
  Channel* channel = ref;
  lovrChannelClear(channel);
  vec_deinit(&channel->messages);
  mtx_destroy(&channel->lock);
  cnd_destroy(&channel->cond);
  free(channel);
}

bool lovrChannelPush(Channel* channel, Variant variant, double timeout, uint64_t* id) {
  mtx_lock(&channel->lock);
  if (channel->messages.length == 0) {
    lovrRetain(channel);
  }
  vec_insert(&channel->messages, 0, variant);
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
      timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double) 1e9;
    }
  }

  bool read = channel->received >= *id;
  mtx_unlock(&channel->lock);
  return read;
}

bool lovrChannelPop(Channel* channel, Variant* variant, double timeout) {
  mtx_lock(&channel->lock);

  do {
    if (channel->messages.length > 0) {
      *variant = vec_pop(&channel->messages);
      if (channel->messages.length == 0) {
        lovrRelease(channel);
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
  } while (true);
}

bool lovrChannelPeek(Channel* channel, Variant* variant) {
  mtx_lock(&channel->lock);

  if (channel->messages.length > 0) {
    *variant = vec_last(&channel->messages);
    mtx_unlock(&channel->lock);
    return true;
  }

  mtx_unlock(&channel->lock);
  return false;
}

void lovrChannelClear(Channel* channel) {
  mtx_lock(&channel->lock);
  for (int i = 0; i < channel->messages.length; i++) {
    Variant variant = channel->messages.data[i];
    if (variant.type == TYPE_STRING) {
      free(variant.value.string);
    } else if (variant.type == TYPE_OBJECT) {
      lovrRelease(variant.value.ref);
    }
  }
  channel->received = channel->sent;
  vec_clear(&channel->messages);
  cnd_broadcast(&channel->cond);
  mtx_unlock(&channel->lock);
}

uint64_t lovrChannelGetCount(Channel* channel) {
  mtx_lock(&channel->lock);
  uint64_t length = channel->messages.length;
  mtx_unlock(&channel->lock);
  return length;
}

bool lovrChannelHasRead(Channel* channel, uint64_t id) {
  mtx_lock(&channel->lock);
  bool received = channel->received >= id;
  mtx_unlock(&channel->lock);
  return received;
}
