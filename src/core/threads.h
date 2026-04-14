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

// Atomics
typedef SDL_AtomicInt lovr_atomic_bool;
typedef SDL_AtomicU32 lovr_atomic_uint;
typedef struct {
  SDL_SpinLock lock;
  uint64_t value;
} lovr_atomic_u64;
#define lovr_atomic_ptr(T) T*

static inline bool lovr_atomic_bool_load(lovr_atomic_bool* ptr) {
  return SDL_GetAtomicInt(ptr) != 0;
}

static inline void lovr_atomic_bool_store(lovr_atomic_bool* ptr, bool value) {
  SDL_SetAtomicInt(ptr, value);
}

static inline bool lovr_atomic_bool_exchange(lovr_atomic_bool* ptr, bool value) {
  int old;
  do {
    old = SDL_GetAtomicInt(ptr);
  } while (!SDL_CompareAndSwapAtomicInt(ptr, old, value));
  return old != 0;
}

static inline uint32_t lovr_atomic_load(lovr_atomic_uint* ptr) {
  return SDL_GetAtomicU32(ptr);
}

static inline void lovr_atomic_store(lovr_atomic_uint* ptr, uint32_t value) {
  SDL_SetAtomicU32(ptr, value);
}

static inline uint32_t lovr_atomic_fetch_add(lovr_atomic_uint* ptr, uint32_t value) {
  return SDL_AddAtomicU32(ptr, value);
}

static inline uint32_t lovr_atomic_fetch_sub(lovr_atomic_uint* ptr, uint32_t value) {
  return SDL_AddAtomicU32(ptr, -(int) value);
}

static inline uint32_t lovr_atomic_exchange(lovr_atomic_uint* ptr, uint32_t value) {
  uint32_t old;
  do {
    old = SDL_GetAtomicU32(ptr);
  } while (!SDL_CompareAndSwapAtomicU32(ptr, old, value));
  return old;
}

static inline bool lovr_atomic_compare_exchange(lovr_atomic_uint* ptr, uint32_t* expected, uint32_t value) {
  uint32_t old = *expected;
  if (!SDL_CompareAndSwapAtomicU32(ptr, old, value)) {
    *expected = SDL_GetAtomicU32(ptr);
    return false;
  }
  return true;
}

static inline uint32_t lovr_atomic_fetch_or(lovr_atomic_uint* ptr, uint32_t mask) {
  uint32_t value;
  do {
    value = SDL_GetAtomicU32(ptr);
  } while (!SDL_CompareAndSwapAtomicU32(ptr, value, value | mask));
  return value;
}

static inline uint32_t lovr_atomic_fetch_xor(lovr_atomic_uint* ptr, uint32_t mask) {
  uint32_t value;
  do {
    value = SDL_GetAtomicU32(ptr);
  } while (!SDL_CompareAndSwapAtomicU32(ptr, value, value ^ mask));
  return value;
}

static inline uint64_t lovr_atomic_u64_load(lovr_atomic_u64* ptr) {
  uint64_t value;
  SDL_LockSpinlock(&ptr->lock);
  value = ptr->value;
  SDL_UnlockSpinlock(&ptr->lock);
  return value;
}

static inline void lovr_atomic_u64_store(lovr_atomic_u64* ptr, uint64_t value) {
  SDL_LockSpinlock(&ptr->lock);
  ptr->value = value;
  SDL_UnlockSpinlock(&ptr->lock);
}

static inline uint64_t lovr_atomic_u64_exchange(lovr_atomic_u64* ptr, uint64_t value) {
  uint64_t old;
  SDL_LockSpinlock(&ptr->lock);
  old = ptr->value;
  ptr->value = value;
  SDL_UnlockSpinlock(&ptr->lock);
  return old;
}

static inline uint64_t lovr_atomic_u64_fetch_or(lovr_atomic_u64* ptr, uint64_t mask) {
  uint64_t old;
  SDL_LockSpinlock(&ptr->lock);
  old = ptr->value;
  ptr->value |= mask;
  SDL_UnlockSpinlock(&ptr->lock);
  return old;
}

static inline uint64_t lovr_atomic_u64_fetch_and(lovr_atomic_u64* ptr, uint64_t mask) {
  uint64_t old;
  SDL_LockSpinlock(&ptr->lock);
  old = ptr->value;
  ptr->value &= mask;
  SDL_UnlockSpinlock(&ptr->lock);
  return old;
}

static inline void* lovr_atomic_ptr_load_impl(void** ptr) {
  return SDL_GetAtomicPointer(ptr);
}

static inline void lovr_atomic_ptr_store_impl(void** ptr, void* value) {
  SDL_SetAtomicPointer(ptr, value);
}

static inline void* lovr_atomic_ptr_exchange_impl(void** ptr, void* value) {
  return SDL_SetAtomicPointer(ptr, value);
}

static inline bool lovr_atomic_ptr_compare_exchange_impl(void** ptr, void** expected, void* value) {
  void* old = *expected;
  if (!SDL_CompareAndSwapAtomicPointer(ptr, old, value)) {
    *expected = SDL_GetAtomicPointer(ptr);
    return false;
  }
  return true;
}

#define lovr_atomic_ptr_load(ptr) lovr_atomic_ptr_load_impl((void**) (ptr))
#define lovr_atomic_ptr_store(ptr, value) lovr_atomic_ptr_store_impl((void**) (ptr), (void*) (value))
#define lovr_atomic_ptr_exchange(ptr, value) lovr_atomic_ptr_exchange_impl((void**) (ptr), (void*) (value))
#define lovr_atomic_ptr_compare_exchange(ptr, expected, value) lovr_atomic_ptr_compare_exchange_impl((void**) (ptr), (void**) (expected), (void*) (value))

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

// Atomics (C11)
#include <stdatomic.h>
typedef atomic_bool lovr_atomic_bool;
typedef atomic_uint lovr_atomic_uint;
typedef _Atomic(uint64_t) lovr_atomic_u64;
#define lovr_atomic_ptr(T) _Atomic(T*)
#define lovr_atomic_bool_load(ptr) atomic_load(ptr)
#define lovr_atomic_bool_store(ptr, value) atomic_store(ptr, value)
#define lovr_atomic_bool_exchange(ptr, value) atomic_exchange(ptr, value)
#define lovr_atomic_load(ptr) atomic_load(ptr)
#define lovr_atomic_store(ptr, value) atomic_store(ptr, value)
#define lovr_atomic_fetch_add(ptr, value) atomic_fetch_add(ptr, value)
#define lovr_atomic_fetch_sub(ptr, value) atomic_fetch_sub(ptr, value)
#define lovr_atomic_exchange(ptr, value) atomic_exchange(ptr, value)
#define lovr_atomic_compare_exchange(ptr, expected, value) atomic_compare_exchange_strong(ptr, expected, value)
#define lovr_atomic_fetch_or(ptr, value) atomic_fetch_or(ptr, value)
#define lovr_atomic_fetch_xor(ptr, value) atomic_fetch_xor(ptr, value)
#define lovr_atomic_u64_load(ptr) atomic_load(ptr)
#define lovr_atomic_u64_store(ptr, value) atomic_store(ptr, value)
#define lovr_atomic_u64_exchange(ptr, value) atomic_exchange(ptr, value)
#define lovr_atomic_u64_fetch_or(ptr, value) atomic_fetch_or(ptr, value)
#define lovr_atomic_u64_fetch_and(ptr, value) atomic_fetch_and(ptr, value)
#define lovr_atomic_ptr_load(ptr) atomic_load(ptr)
#define lovr_atomic_ptr_store(ptr, value) atomic_store(ptr, value)
#define lovr_atomic_ptr_exchange(ptr, value) atomic_exchange(ptr, value)
#define lovr_atomic_ptr_compare_exchange(ptr, expected, value) atomic_compare_exchange_strong(ptr, expected, value)
#endif