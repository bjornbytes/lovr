#include "util.h"
#include <stdint.h>
#include <ode/ode.h>

typedef enum {
  SHAPE_SPHERE,
  SHAPE_BOX,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER
} ShapeType;

typedef struct {
  Ref ref;
  dWorldID id;
} World;

typedef struct {
  Ref ref;
  dBodyID id;
  World* world;
} Body;

typedef struct {
  Ref ref;
  ShapeType type;
  dGeomID id;
  Body* body;
} Shape;

typedef Shape SphereShape;

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
void lovrBodyGetAngularVelocity(Body* body, float* x, float* y, float* z);
void lovrBodySetAngularVelocity(Body* body, float x, float y, float z);
void lovrBodyGetLinearDamping(Body* body, float* damping, float* threshold);
void lovrBodySetLinearDamping(Body* body, float damping, float threshold);
void lovrBodyGetAngularDamping(Body* body, float* damping, float* threshold);
void lovrBodySetAngularDamping(Body* body, float damping, float threshold);
void lovrBodyApplyForce(Body* body, float x, float y, float z);
void lovrBodyApplyForceAtPosition(Body* body, float x, float y, float z, float cx, float cy, float cz);
void lovrBodyApplyTorque(Body* body, float x, float y, float z);
int lovrBodyIsKinematic(Body* body);
void lovrBodySetKinematic(Body* body, int kinematic);
void lovrBodyGetLocalPoint(Body* body, float wx, float wy, float wz, float* x, float* y, float* z);
void lovrBodyGetWorldPoint(Body* body, float x, float y, float z, float* wx, float* wy, float* wz);
void lovrBodyGetLocalVector(Body* body, float wx, float wy, float wz, float* x, float* y, float* z);
void lovrBodyGetWorldVector(Body* body, float x, float y, float z, float* wx, float* wy, float* wz);
void lovrBodyGetLinearVelocityFromLocalPoint(Body* body, float x, float y, float z, float* vx, float* vy, float* vz);
void lovrBodyGetLinearVelocityFromWorldPoint(Body* body, float wx, float wy, float wz, float* vx, float* vy, float* vz);
int lovrBodyIsSleepingAllowed(Body* body);
void lovrBodySetSleepingAllowed(Body* body, int allowed);
int lovrBodyIsAwake(Body* body);
void lovrBodySetAwake(Body* body, int awake);
void* lovrBodyGetUserData(Body* body);
void lovrBodySetUserData(Body* body, void* data);
World* lovrBodyGetWorld(Body* body);

void lovrShapeDestroy(const Ref* ref);
ShapeType lovrShapeGetType(Shape* shape);
Body* lovrShapeGetBody(Shape* shape);
void lovrShapeSetBody(Shape* shape, Body* body);
int lovrShapeIsEnabled(Shape* shape);
void lovrShapeSetEnabled(Shape* shape, int enabled);
void* lovrShapeGetUserData(Shape* shape);
void lovrShapeSetUserData(Shape* shape, void* data);
void lovrShapeGetPosition(Shape* shape, float* x, float* y, float* z);
void lovrShapeSetPosition(Shape* shape, float x, float y, float z);
void lovrShapeGetOrientation(Shape* shape, float* angle, float* x, float* y, float* z);
void lovrShapeSetOrientation(Shape* shape, float angle, float x, float y, float z);
uint32_t lovrShapeGetCategory(Shape* shape);
void lovrShapeSetCategory(Shape* shape, uint32_t category);
uint32_t lovrShapeGetMask(Shape* shape);
void lovrShapeSetMask(Shape* shape, uint32_t mask);

SphereShape* lovrSphereShapeCreate(float radius);
float lovrSphereShapeGetRadius(SphereShape* shape);
void lovrSphereShapeSetRadius(SphereShape* shape, float radius);
