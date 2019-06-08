#include "event/event.h"
#include "core/arr.h"
#include "lib/tinycthread/tinycthread.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct Channel {
  mtx_t lock;
  cnd_t cond;
  arr_t(Variant, 1) messages;
  size_t head;
  uint64_t sent;
  uint64_t received;
} Channel;

Channel* lovrChannelInit(Channel* channel);
#define lovrChannelCreate() lovrChannelInit(lovrAlloc(Channel))
void lovrChannelDestroy(void* ref);
bool lovrChannelPush(Channel* channel, Variant variant, double timeout, uint64_t* id);
bool lovrChannelPop(Channel* channel, Variant* variant, double timeout);
bool lovrChannelPeek(Channel* channel, Variant* variant);
void lovrChannelClear(Channel* channel);
uint64_t lovrChannelGetCount(Channel* channel);
bool lovrChannelHasRead(Channel* channel, uint64_t id);
