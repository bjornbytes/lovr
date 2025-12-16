#include "task/task.h"
#include "core/job.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

static atomic_uint ref;

static struct {
  Waiter* waiters;
  _Atomic(Task*) pending;
  Task* queue;
} state;

bool lovrTaskModuleInit(void) {
  if (!lovrModuleAcquire(&ref)) return false;
  lovrModuleReady(&ref);
  return true;
}

void lovrTaskModuleDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  while (state.waiters) {
    Waiter* waiter = state.waiters;
    state.waiters = waiter->next;
    lovrFree(waiter);
  }
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

Task* lovrTaskModulePoll(void) {
  for (;;) {
    while (state.queue) {
      Task* task = state.queue;
      state.queue = task->next;
      if (!task->dequeued) {
        return task;
      }
    }

    Task* task = atomic_exchange(&state.pending, NULL);

    if (!task) {
      return NULL;
    }

    while (task) {
      Task* next = task->next;
      task->next = state.queue;
      state.queue = task;
      task = next;
    }
  }
}

// Task

Task* lovrTaskCreate(struct lua_State* T) {
  Task* task = lovrCalloc(sizeof(Task));
  task->ref = 1;
  task->T = T;
  return task;
}

void lovrTaskDestroy(void* ref) {
  Task* task = ref;
  while (task->waiters) {
    Waiter* waiter = task->waiters;
    task->waiters = waiter->next;
    waiter->next = state.waiters;
    state.waiters = waiter;
  }
  lovrFree(task->error);
  lovrFree(task);
}

bool lovrTaskIsReady(Task* task) {
  return !task->complete && task->deps == 0;
}

void lovrTaskEnqueue(Task* task) {
  task->dequeued = false;
  task->next = atomic_load(&state.pending);
  while (!atomic_compare_exchange_strong(&state.pending, &task->next, task));
}

void lovrTaskDequeue(Task* task) {
  task->dequeued = true;
}

void lovrTaskFinish(Task* task) {
  while (task->waiters) {
    Waiter* waiter = task->waiters;
    task->waiters = waiter->next;

    // If this task failed, copy the error to any dependents
    if (task->error && !waiter->task->error) {
      waiter->task->error = lovrStrdup(task->error);
    }

    if (atomic_fetch_sub(&waiter->task->deps, 1) == 1) {
      lovrTaskEnqueue(waiter->task);
    }

    waiter->next = state.waiters;
    state.waiters = waiter;
  }

  task->complete = true;
}

bool lovrTaskAddDependency(Task* task, Task* dep) {
  if (dep->complete) {
    if (dep->error && !task->error) {
      task->error = lovrStrdup(dep->error);
    }
    return true;
  }

  lovrAssert(task->deps < ~0u, "Task is waiting on too many other tasks");
  Waiter* waiter = state.waiters;

  if (waiter) {
    state.waiters = waiter->next;
  } else {
    waiter = lovrMalloc(sizeof(Waiter));
  }

  waiter->next = dep->waiters;
  waiter->task = task;
  dep->waiters = waiter;
  task->waiting = WAIT_TASK;
  atomic_fetch_add(&task->deps, 1);
  return true;
}
