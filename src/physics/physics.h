#include "util.h"
#include <ode/ode.h>

typedef struct {
  Ref ref;
  dWorldID id;
} World;

typedef struct {
  Ref ref;
  dBodyID id;
  World* world;
} Body;

void lovrPhysicsInit();
void lovrPhysicsDestroy();

World* lovrWorldCreate();
void lovrWorldDestroy(const Ref* ref);
void lovrWorldGetGravity(World* world, float* x, float* y, float* z);
void lovrWorldSetGravity(World* world, float x, float y, float z);
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold);
void lovrWorldSetLinearDamping(World* world, float damping, float threshold);
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold);
void lovrWorldSetAngularDamping(World* world, float damping, float threshold);
int lovrWorldIsSleepingAllowed(World* world);
void lovrWorldSetSleepingAllowed(World* world, int allowed);
void lovrWorldUpdate(World* world, float dt);

Body* lovrBodyCreate();
void lovrBodyDestroy(const Ref* ref);
void lovrBodyGetPosition(Body* body, float* x, float* y, float* z);
void lovrBodySetPosition(Body* body, float x, float y, float z);
void lovrBodyGetOrientation(Body* body, float* angle, float* x, float* y, float* z);
void lovrBodySetOrientation(Body* body, float angle, float x, float y, float z);
void lovrBodyGetLinearVelocity(Body* body, float* x, float* y, float* z);
void lovrBodySetLinearVelocity(Body* body, float x, float y, float z);
