#include "event/event.h"
#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include "lib/vec/vec.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Channel {
  Ref ref;
  mtx_t lock;
  cnd_t cond;
  vec_variant_t messages;
  uint64_t sent;
  uint64_t received;
};

Channel* lovrChannelInit(Channel* channel);
#define lovrChannelCreate() lovrChannelInit(lovrAlloc(Channel, lovrChannelDestroy))
void lovrChannelDestroy(void* ref);
bool lovrChannelPush(Channel* channel, Variant variant, double timeout, uint64_t* id);
bool lovrChannelPop(Channel* channel, Variant* variant, double timeout);
bool lovrChannelPeek(Channel* channel, Variant* variant);
void lovrChannelClear(Channel* channel);
uint64_t lovrChannelGetCount(Channel* channel);
bool lovrChannelHasRead(Channel* channel, uint64_t id);
