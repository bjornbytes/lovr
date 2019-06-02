#include "util.h"
#include "platform.h"
#include <stdlib.h>

LOVR_THREAD_LOCAL errorFn* lovrErrorCallback = NULL;
LOVR_THREAD_LOCAL void* lovrErrorUserdata = NULL;

void lovrSetErrorCallback(errorFn* callback, void* userdata) {
  lovrErrorCallback = callback;
  lovrErrorUserdata = userdata;
}

void lovrThrow(const char* format, ...) {
  if (lovrErrorCallback) {
    va_list args;
    va_start(args, format);
    lovrErrorCallback(lovrErrorUserdata, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
  } else {
    va_list args;
    va_start(args, format);
    lovrWarn("Error: ");
    lovrWarnv(format, args);
    lovrWarn("\n");
    va_end(args);
    exit(EXIT_FAILURE);
  }
}

uint32_t hash(const char* str) {
  uint32_t x = 0;
  while (*str) {
    x = (x * 65599) + *str++;
  }
  return x;
}
