#include "timer.h"
#include "../glfw.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

static TimerState timerState;

void lovrTimerInit() {
  lovrTimerStep();
}

double lovrTimerGetDelta() {
  return timerState.dt;
}

double lovrTimerGetTime() {
  return glfwGetTime();
}

double lovrTimerStep() {
  timerState.lastTime = timerState.time;
  timerState.time = glfwGetTime();
  timerState.dt = timerState.time - timerState.lastTime;
  return timerState.dt;
}

void lovrTimerSleep(double seconds) {
#ifdef _WIN32
  Sleep((unsigned int)(seconds * 1000));
#else
  usleep((unsigned int)(seconds * 1000000));
#endif
}
