#include "timer.h"
#include "../glfw.h"

double lovrTimerStep() {
  double time = glfwGetTime();
  glfwSetTime(0);
  return time;
}
