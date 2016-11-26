#include "timer/timer.h"
#include "glfw.h"
#include "util.h"

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
  lovrSleep(seconds);
}
