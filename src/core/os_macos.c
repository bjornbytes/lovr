#include "os.h"
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <time.h>

#include "os_glfw.h"

static uint64_t epoch;
static uint64_t frequency;

static uint64_t getTime() {
  return mach_absolute_time();
}

bool lovrPlatformInit() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  frequency = (info.denom * 1e9) / info.numer;
  epoch = getTime();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "macOS";
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) frequency;
}

void lovrPlatformSetTime(double t) {
  epoch = getTime() - (uint64_t) (t * frequency + .5);
}

void lovrPlatformSleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9;
  while (nanosleep(&t, &t));
}

void lovrPlatformOpenConsole() {
  //
}
