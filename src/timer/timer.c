#include "timer.h"
#include "../glfw.h"
#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

double lovrTimerStep() {
  double time = glfwGetTime();
  glfwSetTime(0);
  return time;
}

void lovrTimerSleep(double seconds) {
#ifdef WIN32
  Sleep((unsigned int)(seconds * 1000));
#else
  usleep((unsigned int)(seconds * 1000000));
#endif
}
