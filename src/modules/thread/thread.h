#include "data/blob.h"
#include "event/event.h"
#include "lib/tinycthread/tinycthread.h"
#include <stdbool.h>
#include <stdint.h>

// Note: Channels retrieved with lovrThreadGetChannel don't need to be released.  They're just all
// cleaned up when the thread module is destroyed.

#pragma once

#define MAX_THREAD_ARGUMENTS 4

struct Channel;

typedef struct Thread {
  thrd_t handle;
  mtx_t lock;
  Blob* body;
  Variant arguments[MAX_THREAD_ARGUMENTS];
  size_t argumentCount;
  int (*runner)(void*);
  char* error;
  bool running;
} Thread;

bool lovrThreadModuleInit(void);
void lovrThreadModuleDestroy(void);
struct Channel* lovrThreadGetChannel(const char* name);
void lovrThreadRemoveChannel(uint64_t hash);

Thread* lovrThreadInit(Thread* thread, int (*runner)(void*), Blob* body);
#define lovrThreadCreate(...) lovrThreadInit(lovrAlloc(Thread), __VA_ARGS__)
void lovrThreadDestroy(void* ref);
void lovrThreadStart(Thread* thread, Variant* arguments, size_t argumentCount);
void lovrThreadWait(Thread* thread);
const char* lovrThreadGetError(Thread* thread);
bool lovrThreadIsRunning(Thread* thread);
