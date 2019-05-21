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

int lovrPlatformGetExecutablePath(char* dest, uint32_t size) {
  return _NSGetExecutablePath(dest, &size);
}

// TODO: Actually, this could perfectly well return the bundle ID, but who would need it?
sds lovrPlatformGetApplicationId() {
	return NULL;
}
