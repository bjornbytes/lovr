#include "job.h"
#include "core/threads.h"
#include <string.h>

#define MAX_WORKERS 64
#define MAX_JOBS 4096
#define JOB_MASK (MAX_JOBS - 1)

typedef struct {
  fn_job* fn;
  void* arg;
} job;

static struct {
  uint32_t head;
  uint32_t tail;
  job jobs[MAX_JOBS];
  lovr_thread workers[MAX_WORKERS];
  uint32_t workerCount;
  fn_hook* workerInit;
  fn_hook* workerQuit;
  lovr_cond hasJob;
  lovr_mutex lock;
  bool quit;
} state;

static void runJob(void) {
  job job = state.jobs[state.head++ & JOB_MASK];
  lovr_mutex_unlock(&state.lock);
  job.fn(job.arg);
}

static int workerLoop(void* arg) {
  uint32_t id = (uint32_t) (uintptr_t) arg;

  if (state.workerInit) {
    state.workerInit(id);
  }

  for (;;) {
    lovr_mutex_lock(&state.lock);

    while (state.head == state.tail && !state.quit) {
      lovr_cond_wait(&state.hasJob, &state.lock);
    }

    if (state.quit) {
      break;
    }

    runJob();
  }

  lovr_mutex_unlock(&state.lock);

  if (state.workerQuit) {
    state.workerQuit(id);
  }

  return 0;
}

bool job_init(uint32_t count, fn_hook* init, fn_hook* quit) {
  lovr_mutex_create(&state.lock);
  lovr_cond_create(&state.hasJob);

  state.workerInit = init;
  state.workerQuit = quit;
  if (count > MAX_WORKERS) count = MAX_WORKERS;
  for (uint32_t i = 0; i < count; i++, state.workerCount++) {
    if (!lovr_thread_create(&state.workers[i], workerLoop, "worker", (void*) (uintptr_t) i)) {
      return false;
    }
  }

  return true;
}

void job_destroy(void) {
  lovr_mutex_lock(&state.lock);
  state.quit = true;
  lovr_mutex_unlock(&state.lock);
  lovr_cond_broadcast(&state.hasJob);
  for (uint32_t i = 0; i < state.workerCount; i++) {
    lovr_thread_join(state.workers[i]);
  }
  lovr_cond_destroy(&state.hasJob);
  lovr_mutex_destroy(&state.lock);
  memset(&state, 0, sizeof(state));
}

bool job_start(fn_job* fn, void* arg) {
  lovr_mutex_lock(&state.lock);

  if (state.tail - state.head >= MAX_JOBS) {
    lovr_mutex_unlock(&state.lock);
    return false;
  }

  bool empty = state.head == state.tail;
  state.jobs[(state.tail++) & JOB_MASK] = (job) { fn, arg };
  if (empty) lovr_cond_broadcast(&state.hasJob);
  lovr_mutex_unlock(&state.lock);
  return true;
}

void job_spin(void) {
  lovr_mutex_lock(&state.lock);

  if (state.head == state.tail) {
    lovr_mutex_unlock(&state.lock);
    lovr_yield();
  } else {
    runJob();
  }
}
