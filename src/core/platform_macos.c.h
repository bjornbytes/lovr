#include "platform.h"
#include <mach-o/dyld.h>
#include <unistd.h>

#include "platform_glfw.c.h"

const char* lovrPlatformGetName() {
  return "macOS";
}

void lovrPlatformSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}

void lovrPlatformOpenConsole() {
  //
}
