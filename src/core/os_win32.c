#include "os.h"
#include <Windows.h>
#include <stdio.h>

#include "os_glfw.h"

static uint64_t epoch;
static uint64_t frequency;

static uint64_t getTime() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

bool lovrPlatformInit() {
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  frequency = f.QuadPart;
  epoch = getTime();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "Windows";
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) frequency;
}

void lovrPlatformSetTime(double t) {
  epoch = getTime() - (uint64_t) (t * frequency + .5);
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
