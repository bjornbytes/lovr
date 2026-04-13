#include "timer/timer.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

static lovr_atomic_uint ref;

static struct {
  double epoch;
  double lastTime;
  double time;
  double dt;
  int tickIndex;
  double tickSum;
  double tickBuffer[TICK_SAMPLES];
} state;

bool lovrTimerInit(void) {
  if (!lovrModuleAcquire(&ref)) return true;
  state.epoch = os_get_time();
  lovrModuleReady(&ref);
  return true;
}

void lovrTimerDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

double lovrTimerGetDelta(void) {
  return state.dt;
}

double lovrTimerGetTime(void) {
  return os_get_time() - state.epoch;
}

double lovrTimerStep(void) {
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

double lovrTimerGetAverageDelta(void) {
  return state.tickSum / TICK_SAMPLES;
}

int lovrTimerGetFPS(void) {
  return (int) (1 / (state.tickSum / TICK_SAMPLES) + .5);
}

void lovrTimerSleep(double seconds) {
  os_sleep(seconds);
}
