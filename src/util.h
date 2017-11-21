#include "lib/vec/vec.h"
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

#pragma once

#define containerof(ptr, type) ((type*)((char*)(ptr) - offsetof(type, ref)))
#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }

typedef vec_t(unsigned int) vec_uint_t;

typedef struct ref {
  void (*free)(const struct ref* ref);
  int count;
} Ref;

typedef struct {
  float r, g, b, a;
} Color;

extern char lovrErrorMessage[];
extern jmp_buf* lovrCatch;

void lovrThrow(const char* format, ...);
void lovrSleep(double seconds);
void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref));
void lovrRetain(const Ref* ref);
void lovrRelease(const Ref* ref);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
