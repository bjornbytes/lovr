#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include <stdbool.h>

#pragma once

typedef struct {
  Ref ref;
  thrd_t handle;
  mtx_t lock;
  int (*runner)(void*);
  const char* body;
  const char* error;
  bool running;
} Thread;

Thread* lovrThreadCreate(int (*runner)(void*), const char* body);
void lovrThreadDestroy(const Ref* ref);
void lovrThreadStart(Thread* thread);
void lovrThreadWait(Thread* thread);
const char* lovrThreadGetError(Thread* thread);
bool lovrThreadIsRunning(Thread* thread);
