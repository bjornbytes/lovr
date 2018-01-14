#include "timer/timer.h"
#include "lib/glfw.h"
#include "util.h"

static TimerState timerState;

void lovrTimerInit() {
  timerState.tickIndex = 0;
  for (int i = 0; i < TICK_SAMPLES; i++) {
    timerState.tickBuffer[i] = 0.;
  }
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
  timerState.tickSum -= timerState.tickBuffer[timerState.tickIndex];
  timerState.tickSum += timerState.dt;
  timerState.tickBuffer[timerState.tickIndex] = timerState.dt;
  timerState.averageDelta = timerState.tickSum / TICK_SAMPLES;
  timerState.fps = (int) (1 / (timerState.tickSum / TICK_SAMPLES) + .5);
  if (++timerState.tickIndex == TICK_SAMPLES) {
    timerState.tickIndex = 0;
  }
  return timerState.dt;
}

double lovrTimerGetAverageDelta() {
  return timerState.averageDelta;
}

int lovrTimerGetFPS() {
  return timerState.fps;
}

void lovrTimerSleep(double seconds) {
  lovrSleep(seconds);
}
