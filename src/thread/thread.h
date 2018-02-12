#include "util.h"

#pragma once

typedef struct {
  Ref ref;
} Thread;

Thread* lovrThreadCreate();
void lovrThreadDestroy(const Ref* ref);
