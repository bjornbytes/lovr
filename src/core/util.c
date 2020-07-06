#include "util.h"
#include <stdlib.h>
#include <stdio.h>

// Error handling
static void defaultErrorCallback(void* p, const char* format, va_list args) {
  fprintf(stderr, "Error: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

LOVR_THREAD_LOCAL errorFn* lovrErrorCallback = defaultErrorCallback;
LOVR_THREAD_LOCAL void* lovrErrorUserdata = NULL;

void lovrSetErrorCallback(errorFn* callback, void* userdata) {
  lovrErrorCallback = callback ? callback : defaultErrorCallback;
  lovrErrorUserdata = userdata;
}

void lovrThrow(const char* format, ...) {
  va_list args;
  va_start(args, format);
  lovrErrorCallback(lovrErrorUserdata, format, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

// Logging
logFn* lovrLogCallback;
void* lovrLogUserdata;

void lovrSetLogCallback(logFn* callback, void* userdata) {
  lovrLogCallback = callback;
  lovrLogUserdata = userdata;
}

void lovrLog(int level, const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  lovrLogCallback(lovrLogUserdata, level, tag, format, args);
  va_end(args);
}
