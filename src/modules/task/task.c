#include "task/task.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

static atomic_uint ref;

static struct {
  Waiter* waiters;
  _Atomic(Task*) pending;
  Task* queue;
  Task* polls;
  Task* pool;
} state;

bool lovrTaskModuleInit(void) {
  if (!lovrModuleAcquire(&ref)) return false;
  lovrModuleReady(&ref);
  return true;
}

void lovrTaskModuleDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  while (state.pool) {
    Task* task = state.pool;
    state.pool = task->next;
    lovrFree(task);
  }
  while (state.waiters) {
    Waiter* waiter = state.waiters;
    state.waiters = waiter->next;
    lovrFree(waiter);
  }
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

Task* lovrTaskModuleGetNext(void) {
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
      break;
    }

    while (task) {
      Task* next = task->next;
      task->next = state.queue;
      state.queue = task;
      task = next;
    }
  }

  Task** list = &state.polls;
  while (*list) {
    Task* task = *list;
    if (task->fn(&task->context)) {
      *list = task->next;
      if (atomic_fetch_sub(&task->deps, 1) == 1) {
        return task;
      }
    } else {
      list = &task->next;
    }
  }

  return NULL;
}

// Task

Task* lovrTaskCreate(struct lua_State* T) {
  Task* task = state.pool;

  if (task) {
    state.pool = task->next;
    memset(task, 0, sizeof(Task));
  } else {
    task = lovrCalloc(sizeof(Task));
  }

  task->T = T;
  return task;
}

void lovrTaskDestroy(Task* task) {
  while (task->waiters) {
    Waiter* waiter = task->waiters;
    task->waiters = waiter->next;
    waiter->next = state.waiters;
    state.waiters = waiter;
  }
  lovrFree(task->error);
  task->next = state.pool;
  state.pool = task;
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

void lovrTaskWaitPoll(Task* task, fn_task* poll, fn_task* block, fn_continuation* continuation, void* context) {
  task->fn = poll;
  task->block = block;
  task->context = context;
  task->continuation = continuation;
  atomic_store(&task->deps, 1);
  task->waiting = WAIT_POLL;
  task->next = state.polls;
  state.polls = task;
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
