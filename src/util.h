#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 16
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Toad Rage"

#ifdef _MSC_VER
#define LOVR_THREAD_LOCAL __declspec(thread)
#else
#define LOVR_THREAD_LOCAL __thread
#endif

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) (((uintptr_t) (p) + (n - 1)) & ~(n - 1))
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1
#define BREAK() __asm("int $3")

// Error handling
typedef void errorFn(void*, const char*, va_list);
void lovrSetErrorCallback(errorFn* callback, void* userdata);
_Noreturn void lovrThrow(const char* format, ...);
#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
#define lovrUnreachable() lovrThrow("Unreachable")

#ifdef LOVR_UNCHECKED
#define lovrCheck(c, ...) ((void) 0)
#else
#define lovrCheck lovrAssert
#endif

// Logging
typedef void logFn(void*, int, const char*, const char*, va_list);
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
void lovrSetLogCallback(logFn* callback, void* userdata);
void lovrLog(int level, const char* tag, const char* format, ...);

// Hash function (FNV1a)
static inline uint64_t hash64(const void* data, size_t length) {
  const uint8_t* bytes = (const uint8_t*) data;
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < length; i++) {
    hash = (hash ^ bytes[i]) * 0x100000001b3;
  }
  return hash;
}

// Refcounting
void lovrRetain(void* ref);
void lovrRelease(void* ref, void (*destructor)(void*));

// Dynamic Array
typedef void* arr_allocator(void* data, size_t size);
#define arr_t(T) struct { T* data; arr_allocator* alloc; size_t length, capacity; }
#define arr_init(a, allocator) (a)->data = NULL, (a)->length = 0, (a)->capacity = 0, (a)->alloc = allocator
#define arr_free(a) if ((a)->data) (a)->alloc((a)->data, 0)
#define arr_reserve(a, n) _arr_reserve((void**) &((a)->data), n, &(a)->capacity, sizeof(*(a)->data), (a)->alloc)
#define arr_expand(a, n) arr_reserve(a, (a)->length + n)
#define arr_push(a, x) arr_reserve(a, (a)->length + 1), (a)->data[(a)->length] = x, (a)->length++
#define arr_pop(a) (a)->data[--(a)->length]
#define arr_append(a, p, n) arr_reserve(a, (a)->length + n), memcpy((a)->data + (a)->length, p, n * sizeof(*(p))), (a)->length += n
#define arr_splice(a, i, n) memmove((a)->data + (i), (a)->data + ((i) + n), ((a)->length - (i) - (n)) * sizeof(*(a)->data)), (a)->length -= n
#define arr_clear(a) (a)->length = 0

void* arr_alloc(void* data, size_t size);

static inline void _arr_reserve(void** data, size_t n, size_t* capacity, size_t stride, arr_allocator* allocator) {
  if (*capacity >= n) return;
  if (*capacity == 0) *capacity = 1;
  while (*capacity < n) *capacity *= 2;
  *data = allocator(*data, *capacity * stride);
  lovrAssert(*data, "Out of memory");
}

// Hashmap
typedef struct {
  uint64_t* hashes;
  uint64_t* values;
  uint32_t size;
  uint32_t used;
} map_t;

#define MAP_NIL UINT64_MAX

void map_init(map_t* map, uint32_t n);
void map_free(map_t* map);
uint64_t map_get(map_t* map, uint64_t hash);
void map_set(map_t* map, uint64_t hash, uint64_t value);
void map_remove(map_t* map, uint64_t hash);

// UTF-8
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
void utf8_encode(uint32_t codepoint, char str[4]);

// f16
typedef float float32;
typedef uint16_t float16;
void float16Init(void);
float16 float32to16(float32 f);
float32 float16to32(float16 f);
