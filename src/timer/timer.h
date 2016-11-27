#ifndef LOVR_TIMER_TYPES
typedef struct {
  double lastTime;
  double time;
  double dt;
  int frames;
  int fps;
  double lastFpsUpdate;
} TimerState;
#endif

void lovrTimerInit();
double lovrTimerGetDelta();
double lovrTimerGetTime();
double lovrTimerStep();
int lovrTimerGetFPS();
void lovrTimerSleep(double seconds);
