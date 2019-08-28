#include "platform.h"
#include <Windows.h>
#include <stdio.h>

#include "platform_glfw.c.h"

const char* lovrPlatformGetName() {
  return "Windows";
}

void lovrPlatformSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

void lovrPlatformOpenConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stderr);
  }
}

int lovrPlatformGetExecutablePath(char* dest, uint32_t size) {
  return !GetModuleFileName(NULL, dest, size);
}
