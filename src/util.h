#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#pragma once

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#define LOVR_NORETURN __declspec(noreturn)
#define LOVR_THREADLOCAL __declspec(thread)
#else
#define LOVR_EXPORT
#define LOVR_NORETURN __attribute__((noreturn))
#define LOVR_THREADLOCAL __thread
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
extern LOVR_THREADLOCAL lovrErrorHandler lovrErrorCallback;
extern LOVR_THREADLOCAL void* lovrErrorUserdata;

void lovrSetErrorCallback(lovrErrorHandler callback, void* context);
void LOVR_NORETURN lovrThrow(const char* format, ...);
void* _lovrAlloc(const char* type, size_t size, void (*destructor)(void*));
void lovrRetain(void* object);
void lovrRelease(void* object);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
uint32_t nextPo2(uint32_t x);
