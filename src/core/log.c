#include "log.h"

static log_fn* logCallback;
static void* logContext;

#ifdef __ANDROID__

#include <android/log.h>
// TODO

#else

#include <stdio.h>

void log_writev(int level, const char* format, va_list args) {
  vfprintf(stderr, format, args);
}

void log_write(int level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_writev(level, format, args);
  va_end(args);
}

#endif

void log_register(log_fn* callback, void* context) {
  logCallback = callback;
  logContext = context;
}

void log_log(int level, const char* format, ...) {
  va_list args;
  va_start(args, format);

  if (logCallback) {
    logCallback(logContext, level, format, args);
  } else {
    log_writev(level, format, args);
  }

  va_end(args);
}
