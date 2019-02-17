#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include "lib/map/map.h"
#include <stdbool.h>

#pragma once

typedef struct Channel Channel;

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

bool lovrThreadModuleInit(void);
void lovrThreadModuleDestroy(void);
struct Channel* lovrThreadGetChannel(const char* name);

Thread* lovrThreadInit(Thread* thread, int (*runner)(void*), const char* body);
#define lovrThreadCreate(...) lovrThreadInit(lovrAlloc(Thread), __VA_ARGS__)
void lovrThreadDestroy(void* ref);
void lovrThreadStart(Thread* thread);
void lovrThreadWait(Thread* thread);
const char* lovrThreadGetError(Thread* thread);
bool lovrThreadIsRunning(Thread* thread);
