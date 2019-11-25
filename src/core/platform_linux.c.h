#include "platform.h"
#include <unistd.h>
#include <string.h>

#include "platform_glfw.c.h"

const char* lovrPlatformGetName() {
  return "Linux";
}

void lovrPlatformSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}

void lovrPlatformOpenConsole() {
  //
}
