#include "thread/channel.h"
#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include "lib/map/map.h"
#include <stdbool.h>

#pragma once

typedef struct {
  bool initialized;
  map_void_t channels;
} ThreadState;

typedef struct {
  Ref ref;
  thrd_t handle;
  mtx_t lock;
  int (*runner)(void*);
  const char* body;
  const char* error;
  bool running;
} Thread;

void lovrThreadInit();
void lovrThreadDeinit();
Channel* lovrThreadGetChannel(const char* name);

Thread* lovrThreadCreate(int (*runner)(void*), const char* body);
void lovrThreadDestroy(void* ref);
void lovrThreadStart(Thread* thread);
void lovrThreadWait(Thread* thread);
const char* lovrThreadGetError(Thread* thread);
bool lovrThreadIsRunning(Thread* thread);
