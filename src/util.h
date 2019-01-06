#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#pragma once

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#define _Noreturn  __declspec(noreturn)
#define _Thread_local __declspec(thread)
#else
#define LOVR_EXPORT
#define _Noreturn __attribute__((noreturn))
#define _Thread_local __thread
#endif

#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
#define lovrAlloc(T) (T*) _lovrAlloc(#T, sizeof(T), lovr ## T ## Destroy)

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) ((uintptr_t) p & -n)

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
