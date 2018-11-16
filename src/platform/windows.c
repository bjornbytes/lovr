#include "platform.h"
#include <Windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

void lovrPlatformPollEvents() {
  glfwPollEvents();
}

double lovrPlatformGetTime() {
  return glfwGetTime();
}

void lovrPlatformSetTime(double t) {
  glfwSetTime(t);
}

void lovrSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return !GetModuleFileName(NULL, dest, size);
}
