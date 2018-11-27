#include "platform.h"
#include <emscripten.h>

#include "platform/glfw.h"

void lovrSleep(double seconds) {
  emscripten_sleep((unsigned int) (seconds * 1000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return 1;
}

sds lovrGetApplicationId() {
	return NULL;
}
