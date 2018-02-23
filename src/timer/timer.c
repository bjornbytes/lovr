#include "timer/timer.h"
#include "lib/glfw.h"
#include "util.h"

static TimerState state;

void lovrTimerInit() {
  if (state.initialized) return;
  lovrTimerDestroy();
  state.initialized = true;
}

void lovrTimerDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(TimerState));
}

double lovrTimerGetDelta() {
  return state.dt;
}

double lovrTimerGetTime() {
  return glfwGetTime();
}

double lovrTimerStep() {
  state.lastTime = state.time;
  state.time = glfwGetTime();
  state.dt = state.time - state.lastTime;
  state.tickSum -= state.tickBuffer[state.tickIndex];
  state.tickSum += state.dt;
  state.tickBuffer[state.tickIndex] = state.dt;
  state.averageDelta = state.tickSum / TICK_SAMPLES;
  state.fps = (int) (1 / (state.tickSum / TICK_SAMPLES) + .5);
  if (++state.tickIndex == TICK_SAMPLES) {
    state.tickIndex = 0;
  }
  return state.dt;
}

double lovrTimerGetAverageDelta() {
  return state.averageDelta;
}

int lovrTimerGetFPS() {
  return state.fps;
}

void lovrTimerSleep(double seconds) {
  lovrSleep(seconds);
}
