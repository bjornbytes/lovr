#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#pragma once

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#else
#define LOVR_EXPORT
#endif

#ifndef _Noreturn
#ifdef _WIN32
#define _Noreturn  __declspec(noreturn)
#else
#define _Noreturn __attribute__((noreturn))
#endif
#endif

#ifndef _Thread_local
#ifdef _WIN32
#define _Thread_local  __declspec(thread)
#else
#define _Thread_local __thread
#endif
#endif

#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
#define lovrAlloc(T) (T*) _lovrAlloc(#T, sizeof(T), lovr ## T ## Destroy)

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) ((uintptr_t) p & -n)

#define STRLEN(s) (sizeof(s) / sizeof(s[0]) - 1)
#define H1(s, i, x) ((x * 65599u) + (uint8_t) s[(i) < STRLEN(s) ? STRLEN(s) - 1 - (i) : STRLEN(s)])
#define H4(s, i, x) H1(s, i + 0, H1(s, i + 1, H1(s, i + 2, H1(s, i + 3, x))))
#define H16(s, i, x) H4(s, i + 0, H4(s, i + 4, H4(s, i + 8, H4(s, i + 12, x))))
#define H64(s, i, x) H16(s, i + 0, H16(s, i + 16, H16(s, i + 32, H16(s, i + 48, x))))
#define HASH16(s) ((uint32_t) (H16(s, 0, 0) ^ (H16(s, 0, 0) >> 16)))
#define HASH64(s) ((uint32_t) (H64(s, 0, 0) ^ (H64(s, 0, 0) >> 16)))

typedef struct ref {
  void (*destructor)(void*);
  const char* type;
  int count;
} Ref;

typedef struct { float r, g, b, a; } Color;

typedef void (*lovrErrorHandler)(void* userdata, const char* format, va_list args);
extern _Thread_local lovrErrorHandler lovrErrorCallback;
extern _Thread_local void* lovrErrorUserdata;

void lovrSetErrorCallback(lovrErrorHandler callback, void* context);
void _Noreturn lovrThrow(const char* format, ...);
void* _lovrAlloc(const char* type, size_t size, void (*destructor)(void*));
void lovrRetain(void* object);
void lovrRelease(void* object);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
uint32_t nextPo2(uint32_t x);
