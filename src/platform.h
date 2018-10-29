#include <stdint.h>

#pragma once

#include <stdio.h>
#define lovrLog(...) printf(__VA_ARGS__)
#define lovrLogv(...) vprintf(__VA_ARGS__)
#define lovrWarn(...) fprintf(stderr, __VA_ARGS__)
#define lovrWarnv(...) vfprintf(stderr, __VA_ARGS__)

void lovrSleep(double seconds);
int lovrGetExecutablePath(char* dest, uint32_t size);
