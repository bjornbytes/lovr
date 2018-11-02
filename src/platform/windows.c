#include "platform.h"
#include <Windows.h>

void lovrSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return !GetModuleFileName(NULL, dest, size);
}
