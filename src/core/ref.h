#include <stdint.h>
#include <stddef.h>

#pragma once

#if defined(LOVR_ENABLE_THREAD) && defined(_WIN32)

#include <intrin.h>
typedef uint32_t Ref;
static inline uint32_t ref_inc(Ref* ref) { return _InterlockedIncrement(ref); }
static inline uint32_t ref_dec(Ref* ref) { return _InterlockedDecrement(ref); }

#elif defined(LOVR_ENABLE_THREAD)

#include <stdatomic.h>
typedef _Atomic(uint32_t) Ref;
static inline uint32_t ref_inc(Ref* ref) { return atomic_fetch_add(ref, 1) + 1; }
static inline uint32_t ref_dec(Ref* ref) { return atomic_fetch_sub(ref, 1) - 1; }

#else

typedef uint32_t Ref;
static inline uint32_t ref_inc(Ref* ref) { return ++*ref; }
static inline uint32_t ref_dec(Ref* ref) { return --*ref; }

#endif

void* _lovrAlloc(size_t size);
#define toRef(o) (Ref*) (o) - 1
#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T))
#define lovrRetain(o) o && !ref_inc(toRef(o))
#define lovrRelease(T, o) if (o && !ref_dec(toRef(o))) lovr ## T ## Destroy(o), free(toRef(o));
#define _lovrRelease(o, f) if (o && !ref_dec(toRef(o))) f(o), free(toRef(o));
