#include <stdarg.h>
#include <stddef.h>

#pragma once

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 13
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Very Velociraptor"

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
#define ALIGN(p, n) ((uintptr_t) (p) & -n)
#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1

typedef struct Color { float r, g, b, a; } Color;

typedef void errorFn(void*, const char*, va_list);

extern LOVR_THREAD_LOCAL errorFn* lovrErrorCallback;
extern LOVR_THREAD_LOCAL void* lovrErrorUserdata;

void lovrSetErrorCallback(errorFn* callback, void* context);
void LOVR_NORETURN lovrThrow(const char* format, ...);

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
