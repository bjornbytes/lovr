#include <stdbool.h>

#pragma once

#define TICK_SAMPLES 90

typedef struct {
  bool initialized;
  double lastTime;
  double time;
  double dt;
  int tickIndex;
  double tickSum;
  double tickBuffer[TICK_SAMPLES];
  double averageDelta;
  int fps;
} TimerState;

void lovrTimerInit();
void lovrTimerDestroy();
double lovrTimerGetDelta();
double lovrTimerGetTime();
double lovrTimerStep();
double lovrTimerGetAverageDelta();
int lovrTimerGetFPS();
void lovrTimerSleep(double seconds);
