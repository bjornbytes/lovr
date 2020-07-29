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

size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size) {
  const char* path = getenv("HOME");
  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t lovrPlatformGetDataDirectory(char* buffer, size_t size) {
  const char* path = "/home/web_user";
  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t lovrPlatformGetWorkingDirectory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t lovrPlatformGetExecutablePath(char* buffer, size_t size) {
  return 0;
}

size_t lovrPlatformGetBundlePath(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return 0;
}
