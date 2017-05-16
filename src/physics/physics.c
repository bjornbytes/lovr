#include "physics.h"
#include "math/quat.h"
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

World* lovrWorldCreate() {
  World* world = lovrAlloc(sizeof(World), lovrWorldDestroy);
  if (!world) return NULL;

  world->id = dWorldCreate();

  return world;
}

void lovrWorldDestroy(const Ref* ref) {
  World* world = containerof(ref, World);
  dWorldDestroy(world->id);
  free(world);
}

void lovrWorldGetGravity(World* world, float* x, float* y, float* z) {
  dReal gravity[3];
  dWorldGetGravity(world->id, gravity);
  *x = gravity[0];
  *y = gravity[1];
  *z = gravity[2];
}

void lovrWorldSetGravity(World* world, float x, float y, float z) {
  dWorldSetGravity(world->id, x, y, z);
}

void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold) {
  *damping = dWorldGetLinearDamping(world->id);
  *threshold = dWorldGetLinearDampingThreshold(world->id);
}

void lovrWorldSetLinearDamping(World* world, float damping, float threshold) {
  dWorldSetLinearDamping(world->id, damping);
  dWorldSetLinearDampingThreshold(world->id, threshold);
}

void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold) {
  *damping = dWorldGetAngularDamping(world->id);
  *threshold = dWorldGetAngularDampingThreshold(world->id);
}

void lovrWorldSetAngularDamping(World* world, float damping, float threshold) {
  dWorldSetAngularDamping(world->id, damping);
  dWorldSetAngularDampingThreshold(world->id, threshold);
}

int lovrWorldIsSleepingAllowed(World* world) {
  return dWorldGetAutoDisableFlag(world->id);
}

void lovrWorldSetSleepingAllowed(World* world, int allowed) {
  dWorldSetAutoDisableFlag(world->id, allowed);
}

void lovrWorldUpdate(World* world, float dt) {
  dWorldQuickStep(world->id, dt);
}

Body* lovrBodyCreate(World* world) {
  if (!world) {
    error("No world specified");
  }

  Body* body = lovrAlloc(sizeof(Body), lovrBodyDestroy);
  if (!body) return NULL;

  body->id = dBodyCreate(world->id);
  body->world = world;

  return body;
}

void lovrBodyDestroy(const Ref* ref) {
  Body* body = containerof(ref, Body);
  dBodyDestroy(body->id);
  free(body);
}

void lovrBodyGetPosition(Body* body, float* x, float* y, float* z) {
  const dReal* position = dBodyGetPosition(body->id);
  *x = position[0];
  *y = position[1];
  *z = position[2];
}

void lovrBodySetPosition(Body* body, float x, float y, float z) {
  dBodySetPosition(body->id, x, y, z);
}

void lovrBodyGetOrientation(Body* body, float* angle, float* x, float* y, float* z) {
  const dReal* q = dBodyGetQuaternion(body->id);
  float quaternion[4] = { q[0], q[1], q[2], q[3] };
  quat_getAngleAxis(quaternion, angle, x, y, z);
}

void lovrBodySetOrientation(Body* body, float angle, float x, float y, float z) {
  float quaternion[4];
  float axis[3] = { x, y, z };
  dBodySetQuaternion(body->id, quat_fromAngleAxis(quaternion, angle, axis));
}

void lovrBodyGetLinearVelocity(Body* body, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetLinearVel(body->id);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrBodySetLinearVelocity(Body* body, float x, float y, float z) {
  dBodySetLinearVel(body->id, x, y, z);
}

void lovrBodyGetAngularVelocity(Body* body, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetAngularVel(body->id);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrBodySetAngularVelocity(Body* body, float x, float y, float z) {
  dBodySetAngularVel(body->id, x, y, z);
}

void lovrBodyGetLinearDamping(Body* body, float* damping, float* threshold) {
  *damping = dBodyGetLinearDamping(body->id);
  *threshold = dBodyGetLinearDampingThreshold(body->id);
}

void lovrBodySetLinearDamping(Body* body, float damping, float threshold) {
  dBodySetLinearDamping(body->id, damping);
  dBodySetLinearDampingThreshold(body->id, threshold);
}

void lovrBodyGetAngularDamping(Body* body, float* damping, float* threshold) {
  *damping = dBodyGetAngularDamping(body->id);
  *threshold = dBodyGetAngularDampingThreshold(body->id);
}

void lovrBodySetAngularDamping(Body* body, float damping, float threshold) {
  dBodySetAngularDamping(body->id, damping);
  dBodySetAngularDampingThreshold(body->id, threshold);
}

void lovrBodyApplyForce(Body* body, float x, float y, float z) {
  dBodyAddForce(body->id, x, y, z);
}

void lovrBodyApplyForceAtPosition(Body* body, float x, float y, float z, float cx, float cy, float cz) {
  dBodyAddForceAtPos(body->id, x, y, z, cx, cy, cz);
}

void lovrBodyApplyTorque(Body* body, float x, float y, float z) {
  dBodyAddTorque(body->id, x, y, z);
}

int lovrBodyIsKinematic(Body* body) {
  return dBodyIsKinematic(body->id);
}

void lovrBodySetKinematic(Body* body, int kinematic) {
  if (kinematic) {
    dBodySetKinematic(body->id);
  } else {
    dBodySetDynamic(body->id);
  }
}

void lovrBodyGetLocalPoint(Body* body, float wx, float wy, float wz, float* x, float* y, float* z) {
  dReal local[3];
  dBodyGetPosRelPoint(body->id, wx, wy, wz, local);
  *x = local[0];
  *y = local[1];
  *z = local[2];
}

void lovrBodyGetWorldPoint(Body* body, float x, float y, float z, float* wx, float* wy, float* wz) {
  dReal world[3];
  dBodyGetRelPointPos(body->id, x, y, z, world);
  *wx = world[0];
  *wy = world[1];
  *wz = world[2];
}

void lovrBodyGetLocalVector(Body* body, float wx, float wy, float wz, float* x, float* y, float* z) {
  dReal local[3];
  dBodyVectorFromWorld(body->id, wx, wy, wz, local);
  *x = local[0];
  *y = local[1];
  *z = local[2];
}

void lovrBodyGetWorldVector(Body* body, float x, float y, float z, float* wx, float* wy, float* wz) {
  dReal world[3];
  dBodyVectorToWorld(body->id, x, y, z, world);
  *wx = world[0];
  *wy = world[1];
  *wz = world[2];
}

void lovrBodyGetLinearVelocityFromLocalPoint(Body* body, float x, float y, float z, float* vx, float* vy, float* vz) {
  dReal velocity[3];
  dBodyGetRelPointVel(body->id, x, y, z, velocity);
  *vx = velocity[0];
  *vy = velocity[1];
  *vz = velocity[2];
}

void lovrBodyGetLinearVelocityFromWorldPoint(Body* body, float wx, float wy, float wz, float* vx, float* vy, float* vz) {
  dReal velocity[3];
  dBodyGetPointVel(body->id, wx, wy, wz, velocity);
  *vx = velocity[0];
  *vy = velocity[1];
  *vz = velocity[2];
}

int lovrBodyIsSleepingAllowed(Body* body) {
  return dBodyGetAutoDisableFlag(body->id);
}

void lovrBodySetSleepingAllowed(Body* body, int allowed) {
  dBodySetAutoDisableFlag(body->id, allowed);
}

int lovrBodyIsAwake(Body* body) {
  return dBodyIsEnabled(body->id);
}

void lovrBodySetAwake(Body* body, int awake) {
  if (awake) {
    dBodyEnable(body->id);
  } else {
    dBodyDisable(body->id);
  }
}

void* lovrBodyGetUserData(Body* body) {
  return dBodyGetData(body->id);
}

void lovrBodySetUserData(Body* body, void* data) {
  dBodySetData(body->id, data);
}

World* lovrBodyGetWorld(Body* body) {
  return body->world;
}

void lovrShapeDestroy(const Ref* ref) {
  Shape* shape = containerof(ref, Shape);
  dGeomDestroy(shape->id);
  free(shape);
}

ShapeType lovrShapeGetType(Shape* shape) {
  return shape->type;
}

Body* lovrShapeGetBody(Shape* shape) {
  return shape->body;
}

void lovrShapeSetBody(Shape* shape, Body* body) {
  shape->body = body;
  dGeomSetBody(shape->id, body ? body->id : 0);
}

int lovrShapeIsEnabled(Shape* shape) {
  return dGeomIsEnabled(shape->id);
}

void lovrShapeSetEnabled(Shape* shape, int enabled) {
  if (enabled) {
    dGeomEnable(shape->id);
  } else {
    dGeomDisable(shape->id);
  }
}

void* lovrShapeGetUserData(Shape* shape) {
  return dGeomGetData(shape->id);
}

void lovrShapeSetUserData(Shape* shape, void* data) {
  dGeomSetData(shape->id, data);
}

void lovrShapeGetPosition(Shape* shape, float* x, float* y, float* z) {
  const dReal* position = dGeomGetOffsetPosition(shape->id);
  *x = position[0];
  *y = position[1];
  *z = position[2];
}

void lovrShapeSetPosition(Shape* shape, float x, float y, float z) {
  dGeomSetOffsetPosition(shape->id, x, y, z);
}

void lovrShapeGetOrientation(Shape* shape, float* angle, float* x, float* y, float* z) {
  float quaternion[4];
  dGeomGetOffsetQuaternion(shape->id, quaternion);
  quat_getAngleAxis(quaternion, angle, x, y, z);
}

void lovrShapeSetOrientation(Shape* shape, float angle, float x, float y, float z) {
  float quaternion[4];
  float axis[3] = { x, y, z };
  dGeomSetOffsetQuaternion(shape->id, quat_fromAngleAxis(quaternion, angle, axis));
}

uint32_t lovrShapeGetCategory(Shape* shape) {
  return dGeomGetCategoryBits(shape->id);
}

void lovrShapeSetCategory(Shape* shape, uint32_t category) {
  dGeomSetCategoryBits(shape->id, category);
}

uint32_t lovrShapeGetMask(Shape* shape) {
  return dGeomGetCollideBits(shape->id);
}

void lovrShapeSetMask(Shape* shape, uint32_t mask) {
  dGeomSetCollideBits(shape->id, mask);
}

SphereShape* lovrSphereShapeCreate(float radius) {
  SphereShape* sphere = lovrAlloc(sizeof(SphereShape), lovrShapeDestroy);
  if (!sphere) return NULL;

  sphere->type = SHAPE_SPHERE;
  sphere->id = dCreateSphere(0, radius);

  return sphere;
}

float lovrSphereShapeGetRadius(SphereShape* sphere) {
  return dGeomSphereGetRadius(sphere->id);
}

void lovrSphereShapeSetRadius(SphereShape* sphere, float radius) {
  dGeomSphereSetRadius(sphere->id, radius);
}

BoxShape* lovrBoxShapeCreate(float x, float y, float z) {
  BoxShape* box = lovrAlloc(sizeof(BoxShape), lovrShapeDestroy);
  if (!box) return NULL;

  box->type = SHAPE_BOX;
  box->id = dCreateBox(0, x, y, z);

  return box;
}

void lovrBoxShapeGetDimensions(BoxShape* box, float* x, float* y, float* z) {
  float dimensions[3];
  dGeomBoxGetLengths(box->id, dimensions);
  *x = dimensions[0];
  *y = dimensions[1];
  *z = dimensions[2];
}

void lovrBoxShapeSetDimensions(BoxShape* box, float x, float y, float z) {
  dGeomBoxSetLengths(box->id, x, y, z);
}

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length) {
  CapsuleShape* capsule = lovrAlloc(sizeof(CapsuleShape), lovrShapeDestroy);
  if (!capsule) return NULL;

  capsule->type = SHAPE_CAPSULE;
  capsule->id = dCreateCapsule(0, radius, length);

  return capsule;
}

float lovrCapsuleShapeGetRadius(CapsuleShape* capsule) {
  float radius, length;
  dGeomCapsuleGetParams(capsule->id, &radius, &length);
  return radius;
}

void lovrCapsuleShapeSetRadius(CapsuleShape* capsule, float radius) {
  dGeomCapsuleSetParams(capsule->id, radius, lovrCapsuleShapeGetLength(capsule));
}

float lovrCapsuleShapeGetLength(CapsuleShape* capsule) {
  float radius, length;
  dGeomCapsuleGetParams(capsule->id, &radius, &length);
  return length;
}

void lovrCapsuleShapeSetLength(CapsuleShape* capsule, float length) {
  dGeomCapsuleSetParams(capsule->id, lovrCapsuleShapeGetRadius(capsule), length);
}
