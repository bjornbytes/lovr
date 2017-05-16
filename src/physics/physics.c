#include "physics.h"
#include <stdlib.h>

void lovrPhysicsInit() {
  dInitODE();

  if (!dCheckConfiguration("ODE_single_precision")) {
    error("lovr.physics must use single precision");
  }

  atexit(lovrPhysicsDestroy);
}

void lovrPhysicsDestroy() {
  dCloseODE();
}
