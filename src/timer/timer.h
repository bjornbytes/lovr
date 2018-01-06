#pragma once

#define TICK_SAMPLES 90

typedef struct {
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
double lovrTimerGetDelta();
double lovrTimerGetTime();
double lovrTimerStep();
double lovrTimerGetAverageDelta();
int lovrTimerGetFPS();
void lovrTimerSleep(double seconds);
