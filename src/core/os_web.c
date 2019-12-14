#include "os.h"
#include <emscripten.h>

#include "os_glfw.h"

static double epoch;
#define TIMER_FREQUENCY 1000

bool lovrPlatformInit() {
  epoch = emscripten_get_now();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "Web";
}

double lovrPlatformGetTime() {
  return (emscripten_get_now() - epoch) / TIMER_FREQUENCY;
}

void lovrPlatformSetTime(double t) {
  epoch = emscripten_get_now() - (t * TIMER_FREQUENCY);
}

void lovrPlatformSleep(double seconds) {
  emscripten_sleep((unsigned int) (seconds * 1000 + .5));
}

void lovrPlatformOpenConsole() {
  //
}
