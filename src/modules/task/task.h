#include <stdbool.h>
#include <stdatomic.h>

#pragma once

struct lua_State;
typedef bool fn_task(void** context);
typedef int fn_continuation(struct lua_State* L, bool success, void* context);

typedef struct Task Task;

bool lovrTaskModuleInit(void);
void lovrTaskModuleDestroy(void);
Task* lovrTaskModuleGetNext(void);

// Task

typedef struct Waiter {
  struct Waiter* next;
  Task* task;
} Waiter;

typedef enum {
  WAIT_NONE,
  WAIT_TASK,
  WAIT_POLL,
  WAIT_JOB
} WaitType;

struct Task {
  bool complete;
  bool dequeued;
  WaitType waiting;
  atomic_uint deps;
  struct Task* next;
  struct lua_State* T;
  fn_task* fn;
  fn_task* block;
  fn_continuation* continuation;
  void* context;
  Waiter* waiters;
  _Atomic(char*) error;
};

Task* lovrTaskCreate(struct lua_State* T);
void lovrTaskDestroy(Task* task);
bool lovrTaskIsReady(Task* task);
void lovrTaskEnqueue(Task* task);
void lovrTaskDequeue(Task* task);
void lovrTaskWaitPoll(Task* task, fn_task* poll, fn_task* block, fn_continuation* continuation, void* context);
void lovrTaskFinish(Task* task);
bool lovrTaskAddDependency(Task* task, Task* dependency);
