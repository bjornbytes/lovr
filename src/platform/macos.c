#include "platform.h"
#include <mach-o/dyld.h>
#include <unistd.h>

#include "platform/glfw.h"

void lovrSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return _NSGetExecutablePath(dest, &size);
}

// TODO: Actually, this could perfectly well return the bundle ID, but who would need it?
sds lovrGetApplicationId() {
	return NULL;
}
