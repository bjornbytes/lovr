#include "platform.h"
#include <unistd.h>
#include <string.h>
#include <android/log.h>

// Currently Android targets also include the linux.c platform.
// These additional functions are here because stdout/stderr aren't wired up on Android:


void lovrLogv(const char * restrict format, va_list ap) {
  __android_log_vprint(ANDROID_LOG_DEBUG, "LOVR", format, ap);
}

void lovrLog(const char * restrict format, ...) {
  va_list args;
  va_start(args, format);
  lovrLogv(format, args);
  va_end(args);
}

void lovrWarnv(const char * restrict format, va_list ap) {
  __android_log_vprint(ANDROID_LOG_WARN, "LOVR", format, ap);
}

void lovrWarn(const char * restrict format, ...) {
  va_list args;
  va_start(args, format);
  lovrWarnv(format, args);
  va_end(args);
}
