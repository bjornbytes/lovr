#include "util.h"
#include "lib/vec/vec.h"
#include <stdint.h>
#include <ode/ode.h>

#pragma once

#define MAX_CONTACTS 4

typedef enum {
  SHAPE_SPHERE,
  SHAPE_BOX,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER
} ShapeType;

typedef struct {
  Ref ref;
  dWorldID id;
  dSpaceID space;
  dJointGroupID contactGroup;
  vec_void_t overlaps;
} World;

typedef struct {
  Ref ref;
  dBodyID body;
  World* world;
  void* userdata;
} Collider;

typedef struct {
  Ref ref;
  ShapeType type;
  dGeomID id;
  Collider* collider;
  void* userdata;
} Shape;

typedef Shape SphereShape;
typedef Shape BoxShape;
typedef Shape CapsuleShape;
typedef Shape CylinderShape;

typedef void (*CollisionResolver)(World* world, void* userdata);

void lovrPhysicsInit();
void lovrPhysicsDestroy();

World* lovrWorldCreate();
void lovrWorldDestroy(const Ref* ref);
void lovrWorldDestroyData(World* world);
void lovrWorldUpdate(World* world, float dt, CollisionResolver resolver, void* userdata);
void lovrWorldComputeOverlaps(World* world);
int lovrWorldGetNextOverlap(World* world, Shape** a, Shape** b);
int lovrWorldCollide(World* world, Shape* a, Shape* b);
void lovrWorldGetGravity(World* world, float* x, float* y, float* z);
void lovrWorldSetGravity(World* world, float x, float y, float z);
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold);
void lovrWorldSetLinearDamping(World* world, float damping, float threshold);
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold);
void lovrWorldSetAngularDamping(World* world, float damping, float threshold);
int lovrWorldIsSleepingAllowed(World* world);
void lovrWorldSetSleepingAllowed(World* world, int allowed);

Collider* lovrColliderCreate();
void lovrColliderDestroy(const Ref* ref);
void lovrColliderDestroyData(Collider* collider);
World* lovrColliderGetWorld(Collider* collider);
void lovrColliderAddShape(Collider* collider, Shape* shape);
void lovrColliderRemoveShape(Collider* collider, Shape* shape);
void* lovrColliderGetUserData(Collider* collider);
void lovrColliderSetUserData(Collider* collider, void* data);
int lovrColliderIsKinematic(Collider* collider);
void lovrColliderSetKinematic(Collider* collider, int kinematic);
int lovrColliderIsGravityIgnored(Collider* collider);
void lovrColliderSetGravityIgnored(Collider* collider, int ignored);
int lovrColliderIsSleepingAllowed(Collider* collider);
void lovrColliderSetSleepingAllowed(Collider* collider, int allowed);
int lovrColliderIsAwake(Collider* collider);
void lovrColliderSetAwake(Collider* collider, int awake);
float lovrColliderGetMass(Collider* collider);
void lovrColliderSetMass(Collider* collider, float mass);
void lovrColliderGetMassData(Collider* collider, float* cx, float* cy, float* cz, float* mass, float inertia[6]);
void lovrColliderSetMassData(Collider* collider, float cx, float cy, float cz, float mass, float inertia[6]);
void lovrColliderGetPosition(Collider* collider, float* x, float* y, float* z);
void lovrColliderSetPosition(Collider* collider, float x, float y, float z);
void lovrColliderGetOrientation(Collider* collider, float* angle, float* x, float* y, float* z);
void lovrColliderSetOrientation(Collider* collider, float angle, float x, float y, float z);
void lovrColliderGetLinearVelocity(Collider* collider, float* x, float* y, float* z);
void lovrColliderSetLinearVelocity(Collider* collider, float x, float y, float z);
void lovrColliderGetAngularVelocity(Collider* collider, float* x, float* y, float* z);
void lovrColliderSetAngularVelocity(Collider* collider, float x, float y, float z);
void lovrColliderGetLinearDamping(Collider* collider, float* damping, float* threshold);
void lovrColliderSetLinearDamping(Collider* collider, float damping, float threshold);
void lovrColliderGetAngularDamping(Collider* collider, float* damping, float* threshold);
void lovrColliderSetAngularDamping(Collider* collider, float damping, float threshold);
void lovrColliderApplyForce(Collider* collider, float x, float y, float z);
void lovrColliderApplyForceAtPosition(Collider* collider, float x, float y, float z, float cx, float cy, float cz);
void lovrColliderApplyTorque(Collider* collider, float x, float y, float z);
void lovrColliderGetLocalCenter(Collider* collider, float* x, float* y, float* z);
void lovrColliderGetLocalPoint(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z);
void lovrColliderGetWorldPoint(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz);
void lovrColliderGetLocalVector(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z);
void lovrColliderGetWorldVector(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz);
void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float x, float y, float z, float* vx, float* vy, float* vz);
void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float wx, float wy, float wz, float* vx, float* vy, float* vz);

void lovrShapeDestroy(const Ref* ref);
ShapeType lovrShapeGetType(Shape* shape);
Collider* lovrShapeGetCollider(Shape* shape);
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
void lovrShapeComputeMass(Shape* shape, float density, float* cx, float* cy, float* cz, float* mass, float inertia[6]);

SphereShape* lovrSphereShapeCreate(float radius);
float lovrSphereShapeGetRadius(SphereShape* sphere);
void lovrSphereShapeSetRadius(SphereShape* sphere, float radius);

BoxShape* lovrBoxShapeCreate(float x, float y, float z);
void lovrBoxShapeGetDimensions(BoxShape* box, float* x, float* y, float* z);
void lovrBoxShapeSetDimensions(BoxShape* box, float x, float y, float z);

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length);
float lovrCapsuleShapeGetRadius(CapsuleShape* capsule);
void lovrCapsuleShapeSetRadius(CapsuleShape* capsule, float radius);
float lovrCapsuleShapeGetLength(CapsuleShape* capsule);
void lovrCapsuleShapeSetLength(CapsuleShape* capsule, float length);

CylinderShape* lovrCylinderShapeCreate(float radius, float length);
float lovrCylinderShapeGetRadius(CylinderShape* cylinder);
void lovrCylinderShapeSetRadius(CylinderShape* cylinder, float radius);
float lovrCylinderShapeGetLength(CylinderShape* cylinder);
void lovrCylinderShapeSetLength(CylinderShape* cylinder, float length);
