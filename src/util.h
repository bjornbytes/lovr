#include "lib/vec/vec.h"
#include "lib/tinycthread/tinycthread.h"
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

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

extern _Thread_local char lovrErrorMessage[];
extern _Thread_local jmp_buf* lovrCatch;

void lovrThrow(const char* format, ...);
void lovrSleep(double seconds);
void* lovrAlloc(size_t size, void (*destructor)(void* object));
void lovrRetain(void* object);
void lovrRelease(void* object);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
