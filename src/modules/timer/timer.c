#include "timer/timer.h"
#include "platform.h"
#include <string.h>

static struct {
  bool initialized;
  double lastTime;
  double time;
  double dt;
  int tickIndex;
  double tickSum;
  double tickBuffer[TICK_SAMPLES];
  double averageDelta;
  int fps;
} state;

bool lovrTimerInit() {
  if (state.initialized) return false;
  lovrTimerDestroy();
  return state.initialized = true;
}

void lovrTimerDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(state));
}

double lovrTimerGetDelta() {
  return state.dt;
}

double lovrTimerGetTime() {
  return lovrPlatformGetTime();
}

double lovrTimerStep() {
  state.lastTime = state.time;
  state.time = lovrPlatformGetTime();
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
