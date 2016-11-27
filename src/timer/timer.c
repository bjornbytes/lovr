#include "timer/timer.h"
#include "glfw.h"
#include "util.h"

static TimerState timerState;

void lovrTimerInit() {
  timerState.frames = 0;
  timerState.lastFpsUpdate = 0;
  timerState.fps = 0;
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
  timerState.frames++;
  double fpsTimer = timerState.time - timerState.lastFpsUpdate;
  if (fpsTimer > 1) {
    timerState.fps = (int) timerState.frames / fpsTimer + .5;
    timerState.lastFpsUpdate = timerState.time;
    timerState.frames = 0;
  }
  return timerState.dt;
}

int lovrTimerGetFPS() {
  return timerState.fps;
}

void lovrTimerSleep(double seconds) {
  lovrSleep(seconds);
}
