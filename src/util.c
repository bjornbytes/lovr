#include "util.h"
#include "platform.h"
#include <stdlib.h>

_Thread_local lovrErrorHandler lovrErrorCallback = NULL;
_Thread_local void* lovrErrorUserdata = NULL;

void lovrSetErrorCallback(lovrErrorHandler callback, void* userdata) {
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

