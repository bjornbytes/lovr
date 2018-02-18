#include "util.h"

#pragma once

typedef struct {
  Ref ref;
} Channel;

Channel* lovrChannelCreate();
void lovrChannelDestroy(const Ref* ref);
