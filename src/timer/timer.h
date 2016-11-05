#ifndef LOVR_TIMER_TYPES
typedef struct {
  double lastTime;
  double time;
  double dt;
} TimerState;
#endif

void lovrTimerInit();
double lovrTimerGetDelta();
double lovrTimerGetTime();
double lovrTimerStep();
void lovrTimerSleep(double seconds);
