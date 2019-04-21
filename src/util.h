#include <stddef.h>
#include <stdarg.h>

#pragma once

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#else
#define LOVR_EXPORT __attribute__((visibility("default")))
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

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) ((uintptr_t) (p) & -n)

#ifndef M_PI
#define M_PI 3.14159265358979323846264f
#endif

typedef struct Color { float r, g, b, a; } Color;

typedef void (*lovrErrorHandler)(void* userdata, const char* format, va_list args);
extern _Thread_local lovrErrorHandler lovrErrorCallback;
extern _Thread_local void* lovrErrorUserdata;

void lovrSetErrorCallback(lovrErrorHandler callback, void* context);
void _Noreturn lovrThrow(const char* format, ...);

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
