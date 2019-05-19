#include <stdbool.h>

#pragma once

#define TICK_SAMPLES 90

bool lovrTimerInit(void);
void lovrTimerDestroy(void);
double lovrTimerGetDelta(void);
double lovrTimerGetTime(void);
double lovrTimerStep(void);
double lovrTimerGetAverageDelta(void);
int lovrTimerGetFPS(void);
void lovrTimerSleep(double seconds);
