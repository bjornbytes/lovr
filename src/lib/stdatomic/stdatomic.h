// I hacked this together from a C11 draft spec and clang/gcc atomics documentation
// DO NOT TRUST IT

#pragma once

#ifndef _MSC_VER

#include <stdint.h>
#include <stddef.h>

// 7.17.1

#define ATOMIC_BOOL_LOCK_FREE     __GCC_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE     __GCC_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __GCC_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __GCC_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE  __GCC_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE    __GCC_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE      __GCC_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE     __GCC_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE    __GCC_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE  __GCC_ATOMIC_POINTER_LOCK_FREE

// 7.17.2

#define ATOMIC_VAR_INIT(x) (x)
#define atomic_init(p, x) atomic_store_explicit(p, x, memory_order_relaxed)

// 7.17.3

typedef enum memory_order {
  memory_order_relaxed = __ATOMIC_RELAXED,
  memory_order_consume = __ATOMIC_CONSUME,
  memory_order_acquire = __ATOMIC_ACQUIRE,
  memory_order_release = __ATOMIC_RELEASE,
  memory_order_acq_rel = __ATOMIC_ACQ_REL,
  memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#define kill_dependency(x) (x)

// 7.17.4

void atomic_thread_fence(memory_order);
void atomic_signal_fence(memory_order);
#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)

// 7.17.5

#define atomic_is_lock_free(x) __atomic_is_lock_free(sizeof(*(x)), x)

// 7.17.6

typedef _Bool atomic_bool;
typedef char atomic_char;
typedef signed char atomic_schar;
typedef unsigned char atomic_uchar;
typedef short atomic_short;
typedef unsigned short atomic_ushort;
typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef long atomic_long;
typedef unsigned long atomic_ulong;
typedef long long atomic_llong;
typedef unsigned long long atomic_ullong;
typedef uint_least16_t atomic_char16_t;
typedef uint_least32_t atomic_char32_t;
typedef wchar_t atomic_wchar_t;
typedef int_least8_t atomic_int_least8_t;
typedef uint_least8_t atomic_uint_least8_t;
typedef int_least16_t atomic_int_least16_t;
typedef uint_least16_t atomic_uint_least16_t;
typedef int_least32_t atomic_int_least32_t;
typedef uint_least32_t atomic_uint_least32_t;
typedef int_least64_t atomic_int_least64_t;
typedef uint_least64_t atomic_uint_least64_t;
typedef int_fast8_t atomic_int_fast8_t;
typedef uint_fast8_t atomic_uint_fast8_t;
typedef int_fast16_t atomic_int_fast16_t;
typedef uint_fast16_t atomic_uint_fast16_t;
typedef int_fast32_t atomic_int_fast32_t;
typedef uint_fast32_t atomic_uint_fast32_t;
typedef int_fast64_t atomic_int_fast64_t;
typedef uint_fast64_t atomic_uint_fast64_t;
typedef intptr_t atomic_intptr_t;
typedef uintptr_t atomic_uintptr_t;
typedef size_t atomic_size_t;
typedef ptrdiff_t atomic_ptrdiff_t;
typedef intmax_t atomic_intmax_t;
typedef uintmax_t atomic_uintmax_t;

// 7.17.7

#define atomic_store(p, x) __atomic_store(p, x, __ATOMIC_SEQ_CST)
#define atomic_store_explicit __atomic_store

#define atomic_load(p, x) __atomic_load(p, x, __ATOMIC_SEQ_CST)
#define atomic_load_explicit __atomic_load

#define atomic_exchange(p, x) __atomic_exchange(p, x, __ATOMIC_SEQ_CST)
#define atomic_exchange_explicit __atomic_exchange

#define atomic_compare_exchange_strong(p, x, y) __atomic_compare_exchange(p, x, y, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_strong_explicit(p, x, y, o1, o2) __atomic_compare_exchange(p, x, y, false, o1, o2)

#define atomic_compare_exchange_weak(p, x, y) __atomic_compare_exchange(p, x, y, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_weak_explicit(p, x, y, o1, o2) __atomic_compare_exchange(p, x, y, true, o1, o2)

#define atomic_fetch_add(p, x) __atomic_fetch_add(p, x, __ATOMIC_SEQ_CST)
#define atomic_fetch_add_explicit __atomic_fetch_add

#define atomic_fetch_sub(p, x) __atomic_fetch_sub(p, x, __ATOMIC_SEQ_CST)
#define atomic_fetch_sub_explicit __atomic_fetch_sub

#define atomic_fetch_or(p, x) __atomic_fetch_or(p, x, __ATOMIC_SEQ_CST)
#define atomic_fetch_or_explicit __atomic_fetch_or

#define atomic_fetch_xor(p, x) __atomic_fetch_xor(p, x, __ATOMIC_SEQ_CST)
#define atomic_fetch_xor_explicit __atomic_fetch_xor

#define atomic_fetch_and(p, x) __atomic_fetch_and(p, x, __ATOMIC_SEQ_CST)
#define atomic_fetch_and_explicit __atomic_fetch_and

// 7.17.8

#define ATOMIC_FLAG_INIT { 0 }
typedef struct atomic_flag { atomic_bool value; } atomic_flag;

_Bool atomic_flag_test_and_set(volatile atomic_flag*);
_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag*, memory_order);

#define atomic_flag_test_and_set(f) __atomic_test_and_set(f, __ATOMIC_SEQ_CST)
#define atomic_flag_test_and_set_explicit __atomic_test_and_set

_Bool atomic_flag_clear(volatile atomic_flag*);
_Bool atomic_flag_clear_explicit(volatile atomic_flag*, memory_order);

#define atomic_flag_clear(f) __atomic_clear(f, __ATOMIC_SEQ_CST)
#define atomic_flag_clear_explicit __atomic_clear

#else

#include <intrin.h>

typedef volatile long atomic_uint;

#define atomic_fetch_add(p, x) _InterlockedExchangeAdd(p, x)
#define atomic_fetch_sub(p, x) _InterlockedExchangeAdd(p, -(x))

#define ATOMIC_INT_LOCK_FREE 2

#endif
