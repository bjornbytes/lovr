#include <stdint.h>

#pragma once

#ifdef __ANDROID__

#include <stdarg.h>

void lovrLog(const char * restrict format, ...);
void lovrLogv(const char * restrict format, va_list ap);
void lovrWarn(const char * restrict format, ...);
void lovrWarnv(const char * restrict format, va_list ap);

#else

#include <stdio.h>
#define lovrLog(...) printf(__VA_ARGS__)
#define lovrLogv(...) vprintf(__VA_ARGS__)
#define lovrWarn(...) fprintf(stderr, __VA_ARGS__)
#define lovrWarnv(...) vfprintf(stderr, __VA_ARGS__)

#endif

void lovrSleep(double seconds);
int lovrGetExecutablePath(char* dest, uint32_t size);
