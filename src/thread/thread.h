#include "util.h"
#include "lib/tinycthread/tinycthread.h"

#pragma once

typedef struct {
  Ref ref;
  thrd_t handle;
  const char* body;
} Thread;

Thread* lovrThreadCreate(const char* body);
void lovrThreadDestroy(const Ref* ref);
void lovrThreadStart(Thread* thread);
