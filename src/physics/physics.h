#include "util.h"
#include <ode/ode.h>

typedef struct {
  Ref ref;
  dWorldID id;
} World;

void lovrPhysicsInit();
void lovrPhysicsDestroy();

World* lovrWorldCreate();
void lovrWorldDestroy(const Ref* ref);
