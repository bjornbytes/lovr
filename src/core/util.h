#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 14
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Maximum Moss"

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#define LOVR_NORETURN  __declspec(noreturn)
#define LOVR_THREAD_LOCAL  __declspec(thread)
#define LOVR_ALIGN(n) __declspec(align(n))
#define LOVR_INLINE __inline
#define LOVR_RESTRICT __restrict
#else
#define LOVR_EXPORT __attribute__((visibility("default")))
#define LOVR_NORETURN __attribute__((noreturn))
#define LOVR_THREAD_LOCAL __thread
#define LOVR_ALIGN(n) __attribute__((aligned(n)))
#define LOVR_INLINE inline
#define LOVR_RESTRICT restrict
#endif

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) (((uintptr_t) (p) + (n - 1)) & -n)
#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1

typedef struct Color { float r, g, b, a; } Color;

// Error handling
typedef void errorFn(void*, const char*, va_list);
extern LOVR_THREAD_LOCAL errorFn* lovrErrorCallback;
extern LOVR_THREAD_LOCAL void* lovrErrorUserdata;
void lovrSetErrorCallback(errorFn* callback, void* userdata);
void LOVR_NORETURN lovrThrow(const char* format, ...);
#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }

// Logging
typedef void logFn(void*, int, const char*, const char*, va_list);
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
void lovrSetLogCallback(logFn* callback, void* userdata);
void lovrLog(int level, const char* tag, const char* format, ...);

// Hash function (FNV1a)
static LOVR_INLINE uint64_t hash64(const void* data, size_t length) {
  const uint8_t* bytes = (const uint8_t*) data;
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < length; i++) {
    hash = (hash ^ bytes[i]) * 0x100000001b3;
  }
  return hash;
}
