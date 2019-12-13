#include "platform.h"
#include <string.h>
#include <time.h>

#include "platform_glfw.c.h"

const char* lovrPlatformGetName() {
  return "Linux";
}

void lovrPlatformSleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9L;
  while (nanosleep(&t, &t));
}

void lovrPlatformOpenConsole() {
  //
}
