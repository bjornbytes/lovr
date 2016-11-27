#ifndef LOVR_TIMER_TYPES
#define TICK_SAMPLES 60
typedef struct {
  double lastTime;
  double time;
  double dt;
  int tickIndex;
  double tickSum;
  double tickBuffer[TICK_SAMPLES];
  int fps;
} TimerState;
#endif

void lovrTimerInit();
double lovrTimerGetDelta();
double lovrTimerGetTime();
double lovrTimerStep();
int lovrTimerGetFPS();
void lovrTimerSleep(double seconds);
