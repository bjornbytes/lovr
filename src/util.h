#include "lib/vec/vec.h"
#ifdef EMSCRIPTEN
#define _Thread_local
#else
#include "lib/tinycthread/tinycthread.h"
#endif
#include <stdint.h>
#include <stddef.h>

#pragma once

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }

typedef vec_t(unsigned int) vec_uint_t;

typedef struct ref {
  void (*free)(void* object);
  int count;
} Ref;

typedef struct {
  float r, g, b, a;
} Color;

extern _Thread_local void* lovrErrorContext;

void lovrThrow(const char* format, ...);
void lovrSleep(double seconds);
void* lovrAlloc(size_t size, void (*destructor)(void* object));
void lovrRetain(void* object);
void lovrRelease(void* object);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
uint32_t nextPo2(uint32_t x);
