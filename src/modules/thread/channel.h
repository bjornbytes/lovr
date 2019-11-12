#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Variant;

typedef struct Channel Channel;
Channel* lovrChannelCreate(uint64_t hash);
void lovrChannelDestroy(void* ref);
bool lovrChannelPush(Channel* channel, struct Variant* variant, double timeout, uint64_t* id);
bool lovrChannelPop(Channel* channel, struct Variant* variant, double timeout);
bool lovrChannelPeek(Channel* channel, struct Variant* variant);
void lovrChannelClear(Channel* channel);
uint64_t lovrChannelGetCount(Channel* channel);
bool lovrChannelHasRead(Channel* channel, uint64_t id);
