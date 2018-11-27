#include "platform.h"
#include <Windows.h>

#include "platform/glfw.h"

void lovrSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return !GetModuleFileName(NULL, dest, size);
}

sds lovrGetApplicationId() {
	return NULL;
}
