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

bool lovrTimerInit(void);
void lovrTimerDestroy(void);
double lovrTimerGetDelta(void);
double lovrTimerGetTime(void);
double lovrTimerStep(void);
double lovrTimerGetAverageDelta(void);
int lovrTimerGetFPS(void);
void lovrTimerSleep(double seconds);
