#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  exit(EXIT_FAILURE);
}

void lovrSleep(double seconds) {
#ifdef _WIN32
  Sleep((unsigned int)(seconds * 1000));
#else
  usleep((unsigned int)(seconds * 1000000));
#endif
}

void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref)) {
  void* object = malloc(size);
  if (!object) return NULL;
  *((Ref*) object) = (Ref) { destructor, 1 };
  return object;
}

void lovrRetain(const Ref* ref) {
  ((Ref*) ref)->count++;
}

void lovrRelease(const Ref* ref) {
  if (--((Ref*) ref)->count == 0 && ref->free) ref->free(ref);
}
