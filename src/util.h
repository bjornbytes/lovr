#include "lib/vec/vec.h"
#include <stdint.h>
#include <stddef.h>

#pragma once

#define containerof(ptr, type) ((type*)((char*)(ptr) - offsetof(type, ref)))

typedef vec_t(unsigned int) vec_uint_t;

typedef struct ref {
  void (*free)(const struct ref* ref);
  int count;
} Ref;

typedef struct {
  uint8_t r, g, b, a;
} Color;

void error(const char* format, ...);
void lovrSleep(double seconds);
void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref));
void lovrRetain(const Ref* ref);
void lovrRelease(const Ref* ref);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
