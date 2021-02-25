#include "timer/timer.h"
#include "core/os.h"
#include <string.h>

static struct {
  bool initialized;
  double epoch;
  double lastTime;
  double time;
  double dt;
  int tickIndex;
  double tickSum;
  double tickBuffer[TICK_SAMPLES];
} state;

bool lovrTimerInit() {
  if (state.initialized) return false;
  state.initialized = true;
  state.epoch = os_get_time();
  return true;
}

void lovrTimerDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(state));
}

double lovrTimerGetDelta() {
  return state.dt;
}

double lovrTimerGetTime() {
  return os_get_time() - state.epoch;
}

double lovrTimerStep() {
  state.lastTime = state.time;
  state.time = os_get_time();
  state.dt = state.time - state.lastTime;
  state.tickSum -= state.tickBuffer[state.tickIndex];
  state.tickSum += state.dt;
  state.tickBuffer[state.tickIndex] = state.dt;
  if (++state.tickIndex == TICK_SAMPLES) {
    state.tickIndex = 0;
  }
  return state.dt;
}

double lovrTimerGetAverageDelta() {
  return state.tickSum / TICK_SAMPLES;
}

int lovrTimerGetFPS() {
  return (int) (1 / (state.tickSum / TICK_SAMPLES) + .5);
}

void lovrTimerSleep(double seconds) {
  os_sleep(seconds);
}
