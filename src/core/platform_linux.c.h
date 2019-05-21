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

int lovrPlatformGetExecutablePath(char* dest, uint32_t size) {
  memset(dest, 0, size);
  if (readlink("/proc/self/exe", dest, size) != -1) {
    return 0;
  }
  return 1;
}

sds lovrPlatformGetApplicationId() {
	return NULL;
}
