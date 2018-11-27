#include "platform.h"
#include <unistd.h>
#include <string.h>

#include "platform/glfw.h"

void lovrSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  memset(dest, 0, size);
  if (readlink("/proc/self/exe", dest, size) != -1) {
    return 0;
  }
  return 1;
}

sds lovrGetApplicationId() {
	return NULL;
}
