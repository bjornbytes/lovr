#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 17
#define LOVR_VERSION_PATCH 1
#define LOVR_VERSION_ALIAS "Tritium Gourmet"

#ifdef __cplusplus
#define LOVR_NORETURN [[noreturn]]
#else
#define LOVR_NORETURN _Noreturn
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

// Allocation
void* lovrMalloc(size_t size);
void* lovrCalloc(size_t size);
void* lovrRealloc(void* data, size_t size);
void lovrFree(void* data);

// Refcounting (to be refcounted, a struct must have a uint32_t refcount as its first field)
void lovrRetain(void* ref);
void lovrRelease(void* ref, void (*destructor)(void*));

// Errors

const char* lovrGetError(void);
int lovrSetError(const char* format, ...);

#define lovrUnreachable() abort()
#define lovrAssert(c, ...) do { if (!(c)) { lovrSetError(__VA_ARGS__); return 0; } } while (0)
#define lovrAssertGoto(label, c, ...) do { if (!(c)) { lovrSetError(__VA_ARGS__); goto label; } } while (0)
#ifdef LOVR_UNCHECKED
#define lovrCheck(c, ...) ((void) 0)
#define lovrCheckGoto(c, label, ...) ((void) 0)
#else
#define lovrCheck lovrAssert
#define lovrCheckGoto lovrAssertGoto
#endif

// Logging
typedef void fn_log(void*, int, const char*, const char*, va_list);
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
void lovrSetLogCallback(fn_log* callback, void* userdata);
void lovrLog(int level, const char* tag, const char* format, ...);

// Profiling
#ifdef LOVR_PROFILE
#include <TracyC.h>
#define lovrProfileMarkFrame() TracyCFrameMark
#define lovrProfileStart(id, label) TracyCZoneN(id, label, true)
#define lovrProfileEnd(id) TracyCZoneEnd(id)
#define lovrProfileAlloc(p, size) TracyCAlloc(p, size)
#define lovrProfileFree(p) TracyCFree(p)
#else
#define lovrProfileMarkFrame() ((void) 0)
#define lovrProfileStart(id, label) ((void) 0)
#define lovrProfileEnd(id) ((void) 0)
#define lovrProfileAlloc(p, size) ((void) 0)
#define lovrProfileFree(p) ((void) 0)
#endif

// Dynamic Array
#define arr_t(T) struct { T* data; size_t length, capacity; }
#define arr_init(a) (a)->data = NULL, (a)->length = 0, (a)->capacity = 0
#define arr_free(a) if ((a)->data) lovrFree((a)->data)
#define arr_reserve(a, n) _arr_reserve((void**) &((a)->data), n, &(a)->capacity, sizeof(*(a)->data))
#define arr_expand(a, n) arr_reserve(a, (a)->length + n)
#define arr_push(a, x) arr_reserve(a, (a)->length + 1), (a)->data[(a)->length] = x, (a)->length++
#define arr_pop(a) (a)->data[--(a)->length]
#define arr_append(a, p, n) arr_reserve(a, (a)->length + n), memcpy((a)->data + (a)->length, p, n * sizeof(*(p))), (a)->length += n
#define arr_splice(a, i, n) memmove((a)->data + (i), (a)->data + ((i) + n), ((a)->length - (i) - (n)) * sizeof(*(a)->data)), (a)->length -= n
#define arr_clear(a) (a)->length = 0

static inline void _arr_reserve(void** data, size_t n, size_t* capacity, size_t stride) {
  if (*capacity >= n) return;
  if (*capacity == 0) *capacity = 1;
  while (*capacity < n) *capacity *= 2;
  *data = lovrRealloc(*data, *capacity * stride);
}

// Hash function (FNV1a)
static inline uint64_t hash64(const void* data, size_t length) {
  const uint8_t* bytes = (const uint8_t*) data;
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < length; i++) {
    hash = (hash ^ bytes[i]) * 0x100000001b3;
  }
  return hash;
}

// Hashmap (does not support removal)
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

// UTF-8
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
void utf8_encode(uint32_t codepoint, char str[4]);

// f16
typedef float float32;
typedef uint16_t float16;
void float16Init(void);
float16 float32to16(float32 f);
float32 float16to32(float16 f);
