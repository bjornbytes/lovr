#pragma once

#ifdef LOVR_USE_SDL3

#include <SDL3/SDL.h>

typedef SDL_Mutex* lovr_mutex;
typedef SDL_Condition* lovr_cond;
typedef SDL_Thread* lovr_thread;
#define lovr_thread_local _Thread_local

static inline bool lovr_mutex_create(lovr_mutex* m) {
  *m = SDL_CreateMutex();
  return *m != NULL;
}

static inline void lovr_mutex_destroy(lovr_mutex* m) {
  if (*m) { SDL_DestroyMutex(*m); *m = NULL; }
}

static inline void lovr_mutex_lock(lovr_mutex* m) {
  SDL_LockMutex(*m);
}

static inline void lovr_mutex_unlock(lovr_mutex* m) {
  SDL_UnlockMutex(*m);
}

static inline bool lovr_cond_create(lovr_cond* c) {
  *c = SDL_CreateCondition();
  return *c != NULL;
}

static inline void lovr_cond_destroy(lovr_cond* c) {
  if (*c) { SDL_DestroyCondition(*c); *c = NULL; }
}

static inline void lovr_cond_wait(lovr_cond* c, lovr_mutex* m) {
  SDL_WaitCondition(*c, *m);
}

static inline void lovr_cond_broadcast(lovr_cond* c) {
  SDL_BroadcastCondition(*c);
}

static inline void lovr_cond_timedwait(lovr_cond* c, lovr_mutex* m, double* timeout) {
  Sint32 ms = (Sint32)(*timeout * 1000.0);
  if (ms <= 0) ms = 1;
  Uint64 startNs = SDL_GetTicksNS();
  SDL_WaitConditionTimeout(*c, *m, ms);
  *timeout -= (SDL_GetTicksNS() - startNs) / 1e9;
}

static inline bool lovr_thread_create(lovr_thread* t, int (*fn)(void*), const char* name, void* arg) {
  *t = SDL_CreateThread(fn, name, arg);
  return *t != NULL;
}

static inline void lovr_thread_join(lovr_thread t) {
  SDL_WaitThread(t, NULL);
}

static inline void lovr_thread_detach(lovr_thread t) {
  SDL_DetachThread(t);
}

static inline void lovr_yield(void) {
  SDL_DelayNS(0);
}

#else

#include <math.h>
#include <threads.h>
#include <time.h>

typedef mtx_t lovr_mutex;
typedef cnd_t lovr_cond;
typedef thrd_t lovr_thread;
#define lovr_thread_local thread_local

static inline bool lovr_mutex_create(lovr_mutex* m) {
  return mtx_init(m, mtx_plain) == thrd_success;
}

static inline void lovr_mutex_destroy(lovr_mutex* m) {
  mtx_destroy(m);
}

static inline void lovr_mutex_lock(lovr_mutex* m) {
  mtx_lock(m);
}

static inline void lovr_mutex_unlock(lovr_mutex* m) {
  mtx_unlock(m);
}

static inline bool lovr_cond_create(lovr_cond* c) {
  return cnd_init(c) == thrd_success;
}

static inline void lovr_cond_destroy(lovr_cond* c) {
  cnd_destroy(c);
}

static inline void lovr_cond_wait(lovr_cond* c, lovr_mutex* m) {
  cnd_wait(c, m);
}

static inline void lovr_cond_broadcast(lovr_cond* c) {
  cnd_broadcast(c);
}

static inline void lovr_cond_timedwait(lovr_cond* c, lovr_mutex* m, double* timeout) {
  struct timespec start, until, stop;
  timespec_get(&start, TIME_UTC);
  double whole, fraction;
  fraction = modf(*timeout, &whole);
  until.tv_sec = start.tv_sec + (time_t)whole;
  until.tv_nsec = start.tv_nsec + (long)(fraction * 1e9);
  cnd_timedwait(c, m, &until);
  timespec_get(&stop, TIME_UTC);
  *timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / 1e9;
}

static inline bool lovr_thread_create(lovr_thread* t, int (*fn)(void*), const char* name, void* arg) {
  (void) name;
  return thrd_create(t, fn, arg) == thrd_success;
}

static inline void lovr_thread_join(lovr_thread t) {
  thrd_join(t, NULL);
}

static inline void lovr_thread_detach(lovr_thread t) {
  thrd_detach(t);
}

static inline void lovr_yield(void) {
  thrd_yield();
}

#endif
