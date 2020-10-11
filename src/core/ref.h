#include <stdint.h>
#include <stddef.h>

#pragma once

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef LOVR_ENABLE_THREAD

// Thread module is off, don't use atomics

typedef uint32_t Ref;
static inline uint32_t ref_inc(Ref* ref) { return ++*ref; }
static inline uint32_t ref_dec(Ref* ref) { return --*ref; }

#elif defined(_MSC_VER)

// MSVC atomics

#include <intrin.h>
typedef uint32_t Ref;
static inline uint32_t ref_inc(Ref* ref) { return _InterlockedIncrement((volatile long*) ref); }
static inline uint32_t ref_dec(Ref* ref) { return _InterlockedDecrement((volatile long*) ref); }

#elif (defined(__GNUC_MINOR__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))) \
   || (__has_builtin(__atomic_add_fetch) && __has_builtin(__atomic_sub_fetch))

// GCC/Clang atomics

typedef uint32_t Ref;
static inline uint32_t ref_inc(Ref* ref) { return __atomic_add_fetch(ref, 1, __ATOMIC_SEQ_CST); }
static inline uint32_t ref_dec(Ref* ref) { return __atomic_sub_fetch(ref, 1, __ATOMIC_SEQ_CST); }

#else

// No known compiler-specific atomics-- fall back to C11 atomics

// stdatomic.h doesn't work in c++ (except on Android, where it is fine)
#if !defined(__ANDROID__) && defined(__cplusplus)
#error "The header core/ref.h cannot currently be included from C++ when threading is enabled with this compiler. Either remove your ref.h include from any C++ files, or rebuild LOVR with -DLOVR_ENABLE_THREAD=OFF"
#endif

#include <stdatomic.h>
typedef _Atomic(uint32_t) Ref;
static inline uint32_t ref_inc(Ref* ref) { return atomic_fetch_add(ref, 1) + 1; }
static inline uint32_t ref_dec(Ref* ref) { return atomic_fetch_sub(ref, 1) - 1; }

#endif

void* _lovrAlloc(size_t size);
#define toRef(o) ((Ref*) (((char*) (o)) - sizeof(size_t)))
#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T))
#define lovrRetain(o) if (o && !ref_inc(toRef(o))) { lovrThrow("Refcount overflow"); }
#define lovrRelease(T, o) if (o && !ref_dec(toRef(o))) lovr ## T ## Destroy(o), free(toRef(o));
#define _lovrRelease(o, f) if (o && !ref_dec(toRef(o))) f(o), free(toRef(o));
