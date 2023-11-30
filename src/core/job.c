#include "job.h"
#include <stdatomic.h>
#include <threads.h>
#include <string.h>

#define MAX_WORKERS 64
#define MAX_JOBS 1024

struct job {
  job* next;
  atomic_bool done;
  fn_job* fn;
  void* arg;
};

static struct {
  job jobs[MAX_JOBS];
  thrd_t workers[MAX_WORKERS];
  uint32_t workerCount;
  job* freeJob;
  job* nextJob;
  job* lastJob;
  cnd_t hasJob;
  mtx_t lock;
  bool done;
} state;

static int worker_loop(void* arg) {
  for (;;) {
    mtx_lock(&state.lock);
    while (!state.nextJob && !state.done) {
      cnd_wait(&state.hasJob, &state.lock);
    }

    if (state.done) {
      break;
    }

    job* job = state.nextJob;
    state.nextJob = job->next;
    if (!state.nextJob) state.lastJob = NULL;
    mtx_unlock(&state.lock);

    job->fn(job->arg);
    job->done = true;
  }

  mtx_unlock(&state.lock);
  return 0;
}

bool job_init(uint32_t count) {
  if (mtx_init(&state.lock, mtx_plain) != thrd_success) return false;
  if (cnd_init(&state.hasJob) != thrd_success) return false;

  state.freeJob = state.jobs;
  for (uint32_t i = 0; i < MAX_JOBS - 1; i++) {
    state.jobs[i].next = &state.jobs[i + 1];
  }

  if (count > MAX_WORKERS) count = MAX_WORKERS;
  for (uint32_t i = 0; i < count; i++, state.workerCount++) {
    if (thrd_create(&state.workers[i], worker_loop, (void*) (uintptr_t) i) != thrd_success) {
      return false;
    }
  }

  return true;
}

void job_destroy(void) {
  state.done = true;
  cnd_broadcast(&state.hasJob);
  for (uint32_t i = 0; i < state.workerCount; i++) {
    thrd_join(state.workers[i], NULL);
  }
  cnd_destroy(&state.hasJob);
  mtx_destroy(&state.lock);
  memset(&state, 0, sizeof(state));
}

job* job_start(fn_job* fn, void* arg) {
  if (!state.freeJob) {
    return NULL;
  }

  mtx_lock(&state.lock);

  if (!state.freeJob) {
    mtx_unlock(&state.lock);
    return NULL;
  }

  job* job = state.freeJob;
  state.freeJob = job->next;
  if (!state.nextJob) state.nextJob = job;
  if (state.lastJob) state.lastJob->next = job;
  state.lastJob = job;

  job->next = NULL;
  job->done = false;
  job->fn = fn;
  job->arg = arg;

  mtx_unlock(&state.lock);
  cnd_signal(&state.hasJob);
  return job;
}

void job_wait(job* job) {
  mtx_lock(&state.lock);

  while (!job->done) {
    struct job* task = state.nextJob;
    if (task) state.nextJob = task->next;
    if (!state.nextJob) state.lastJob = NULL;
    mtx_unlock(&state.lock);

    if (task) {
      task->fn(task->arg);
      task->done = true;
    } else {
      thrd_yield();
    }

    mtx_lock(&state.lock);
  }

  job->next = state.freeJob;
  state.freeJob = job;

  mtx_unlock(&state.lock);
}
