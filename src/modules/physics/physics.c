#include "physics.h"
#include "core/util.h"
#include <stdlib.h>
#include <stdbool.h>

static void defaultNearCallback(void* data, dGeomID a, dGeomID b) {
  lovrWorldCollide((World*) data, dGeomGetData(a), dGeomGetData(b), -1, -1);
}

static void customNearCallback(void* data, dGeomID shapeA, dGeomID shapeB) {
  World* world = data;
  arr_push(&world->overlaps, dGeomGetData(shapeA));
  arr_push(&world->overlaps, dGeomGetData(shapeB));
}

static void raycastCallback(void* data, dGeomID a, dGeomID b) {
  RaycastCallback callback = ((RaycastData*) data)->callback;
  void* userdata = ((RaycastData*) data)->userdata;
  Shape* shape = dGeomGetData(b);

  if (!shape) {
    return;
  }

  dContact contact;
  if (dCollide(a, b, MAX_CONTACTS, &contact.geom, sizeof(dContact))) {
    dContactGeom g = contact.geom;
    callback(shape, g.pos[0], g.pos[1], g.pos[2], g.normal[0], g.normal[1], g.normal[2], userdata);
  }
}

// XXX slow, but probably fine (tag names are not on any critical path), could switch to hashing if needed
static uint32_t findTag(World* world, const char* name) {
  for (uint32_t i = 0; i < MAX_TAGS && world->tags[i]; i++) {
    if (!strcmp(world->tags[i], name)) {
      return i;
    }
  }
  return NO_TAG;
}

static void onErrorMessage(int num, const char* format, va_list args) {
  char message[1024];
  vsnprintf(message, 1024, format, args);
  lovrLog(LOG_ERROR, "PHY", message);
}

static void onDebugMessage(int num, const char* format, va_list args) {
  char message[1024];
  vsnprintf(message, 1024, format, args);
  lovrLog(LOG_DEBUG, "PHY", message);
}

static void onInfoMessage(int num, const char* format, va_list args) {
  char message[1024];
  vsnprintf(message, 1024, format, args);
  lovrLog(LOG_INFO, "PHY", message);
}

static bool initialized = false;

bool lovrPhysicsInit() {
  if (initialized) return false;
  dInitODE();
  dSetErrorHandler(onErrorMessage);
  dSetDebugHandler(onDebugMessage);
  dSetMessageHandler(onInfoMessage);
  return initialized = true;
}

void lovrPhysicsDestroy() {
  if (!initialized) return;
  dCloseODE();
  initialized = false;
}

World* lovrWorldCreate(float xg, float yg, float zg, bool allowSleep, const char** tags, uint32_t tagCount) {
  World* world = calloc(1, sizeof(World));
  lovrAssert(world, "Out of memory");
  world->ref = 1;
  world->id = dWorldCreate();
  world->space = dHashSpaceCreate(0);
  dHashSpaceSetLevels(world->space, -4, 8);
  world->contactGroup = dJointGroupCreate(0);
  arr_init(&world->overlaps, realloc);
  lovrWorldSetGravity(world, xg, yg, zg);
  lovrWorldSetSleepingAllowed(world, allowSleep);
  for (uint32_t i = 0; i < tagCount; i++) {
    size_t size = strlen(tags[i]) + 1;
    world->tags[i] = malloc(size);
    memcpy(world->tags[i], tags[i], size);
  }
  memset(world->masks, 0xff, sizeof(world->masks));
  return world;
}

void lovrWorldDestroy(void* ref) {
  World* world = ref;
  lovrWorldDestroyData(world);
  arr_free(&world->overlaps);
  for (uint32_t i = 0; i < MAX_TAGS && world->tags[i]; i++) {
    free(world->tags[i]);
  }
  free(world);
}

void lovrWorldDestroyData(World* world) {
  while (world->head) {
    Collider* next = world->head->next;
    lovrColliderDestroyData(world->head);
    world->head = next;
  }

  if (world->contactGroup) {
    dJointGroupDestroy(world->contactGroup);
    world->contactGroup = NULL;
  }

  if (world->space) {
    dSpaceDestroy(world->space);
    world->space = NULL;
  }

  if (world->id) {
    dWorldDestroy(world->id);
    world->id = NULL;
  }
}

void lovrWorldUpdate(World* world, float dt, CollisionResolver resolver, void* userdata) {
  if (resolver) {
    resolver(world, userdata);
  } else {
    dSpaceCollide(world->space, world, defaultNearCallback);
  }

  if (dt > 0) {
    dWorldQuickStep(world->id, dt);
  }

  dJointGroupEmpty(world->contactGroup);
}

void lovrWorldComputeOverlaps(World* world) {
  arr_clear(&world->overlaps);
  dSpaceCollide(world->space, world, customNearCallback);
}

int lovrWorldGetNextOverlap(World* world, Shape** a, Shape** b) {
  if (world->overlaps.length == 0) {
    *a = *b = NULL;
    return 0;
  }

  *a = arr_pop(&world->overlaps);
  *b = arr_pop(&world->overlaps);
  return 1;
}

int lovrWorldCollide(World* world, Shape* a, Shape* b, float friction, float restitution) {
  if (!a || !b) {
    return false;
  }

  Collider* colliderA = a->collider;
  Collider* colliderB = b->collider;
  uint32_t i = colliderA->tag;
  uint32_t j = colliderB->tag;

  if (i != NO_TAG && j != NO_TAG && !((world->masks[i] & (1 << j)) && (world->masks[j] & (1 << i)))) {
    return false;
  }

  if (friction < 0.f) {
    friction = sqrtf(colliderA->friction * colliderB->friction);
  }

  if (restitution < 0.f) {
    restitution = MAX(colliderA->restitution, colliderB->restitution);
  }

  dContact contacts[MAX_CONTACTS];
  for (int c = 0; c < MAX_CONTACTS; c++) {
    contacts[c].surface.mode = 0;
    contacts[c].surface.mu = friction;
    contacts[c].surface.bounce = restitution;
    contacts[c].surface.mu = dInfinity;

    if (restitution > 0) {
      contacts[c].surface.mode |= dContactBounce;
    }
  }

  int contactCount = dCollide(a->id, b->id, MAX_CONTACTS, &contacts[0].geom, sizeof(dContact));

  if (!a->sensor && !b->sensor) {
    for (int c = 0; c < contactCount; c++) {
      dJointID joint = dJointCreateContact(world->id, world->contactGroup, &contacts[c]);
      dJointAttach(joint, colliderA->body, colliderB->body);
    }
  }

  return contactCount;
}

Collider* lovrWorldGetFirstCollider(World* world) {
  return world->head;
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

float lovrWorldGetResponseTime(World* world) {
  return dWorldGetCFM(world->id);
}

void lovrWorldSetResponseTime(World* world, float responseTime) {
  dWorldSetCFM(world->id, responseTime);
}

float lovrWorldGetTightness(World* world) {
  return dWorldGetERP(world->id);
}

void lovrWorldSetTightness(World* world, float tightness) {
  dWorldSetERP(world->id, tightness);
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

bool lovrWorldIsSleepingAllowed(World* world) {
  return dWorldGetAutoDisableFlag(world->id);
}

void lovrWorldSetSleepingAllowed(World* world, bool allowed) {
  dWorldSetAutoDisableFlag(world->id, allowed);
}

void lovrWorldRaycast(World* world, float x1, float y1, float z1, float x2, float y2, float z2, RaycastCallback callback, void* userdata) {
  RaycastData data = { .callback = callback, .userdata = userdata };
  float dx = x2 - x1;
  float dy = y2 - y1;
  float dz = z2 - z1;
  float length = sqrtf(dx * dx + dy * dy + dz * dz);
  dGeomID ray = dCreateRay(world->space, length);
  dGeomRaySet(ray, x1, y1, z1, dx, dy, dz);
  dSpaceCollide2(ray, (dGeomID) world->space, &data, raycastCallback);
  dGeomDestroy(ray);
}

const char* lovrWorldGetTagName(World* world, uint32_t tag) {
  return (tag == NO_TAG) ? NULL : world->tags[tag];
}

int lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == NO_TAG || j == NO_TAG) {
    return NO_TAG;
  }

  world->masks[i] &= ~(1 << j);
  world->masks[j] &= ~(1 << i);
  return 0;
}

int lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == NO_TAG || j == NO_TAG) {
    return NO_TAG;
  }

  world->masks[i] |= (1 << j);
  world->masks[j] |= (1 << i);
  return 0;
}

int lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == NO_TAG || j == NO_TAG) {
    return NO_TAG;
  }

  return (world->masks[i] & (1 << j)) && (world->masks[j] & (1 << i));
}

Collider* lovrColliderCreate(World* world, float x, float y, float z) {
  Collider* collider = calloc(1, sizeof(Collider));
  lovrAssert(collider, "Out of memory");
  collider->ref = 1;
  collider->body = dBodyCreate(world->id);
  collider->world = world;
  collider->friction = 0;
  collider->restitution = 0;
  collider->tag = NO_TAG;
  dBodySetData(collider->body, collider);
  arr_init(&collider->shapes, realloc);
  arr_init(&collider->joints, realloc);

  lovrColliderSetPosition(collider, x, y, z);

  // Adjust the world's collider list
  if (!collider->world->head) {
    collider->world->head = collider;
  } else {
    collider->next = collider->world->head;
    collider->next->prev = collider;
    collider->world->head = collider;
  }

  // The world owns a reference to the collider
  lovrRetain(collider);
  return collider;
}

void lovrColliderDestroy(void* ref) {
  Collider* collider = ref;
  lovrColliderDestroyData(collider);
  arr_free(&collider->shapes);
  arr_free(&collider->joints);
  free(collider);
}

void lovrColliderDestroyData(Collider* collider) {
  if (!collider->body) {
    return;
  }

  size_t count;

  Shape** shapes = lovrColliderGetShapes(collider, &count);
  for (size_t i = 0; i < count; i++) {
    lovrColliderRemoveShape(collider, shapes[i]);
  }

  Joint** joints = lovrColliderGetJoints(collider, &count);
  for (size_t i = 0; i < count; i++) {
    lovrRelease(joints[i], lovrJointDestroy);
  }

  dBodyDestroy(collider->body);
  collider->body = NULL;

  if (collider->next) collider->next->prev = collider->prev;
  if (collider->prev) collider->prev->next = collider->next;
  if (collider->world->head == collider) collider->world->head = collider->next;
  collider->next = collider->prev = NULL;

  // If the Collider is destroyed, the world lets go of its reference to this Collider
  lovrRelease(collider, lovrColliderDestroy);
}

void lovrColliderInitInertia(Collider* collider, Shape* shape) {
  // compute inertia matrix for default density
  const float density = 1.0f;
  float cx, cy, cz, mass, inertia[6];
  lovrShapeGetMass(shape, density, &cx, &cy, &cz, &mass, inertia);
  lovrColliderSetMassData(collider, cx, cy, cz, mass, inertia);
}

World* lovrColliderGetWorld(Collider* collider) {
  return collider->world;
}

void lovrColliderAddShape(Collider* collider, Shape* shape) {
  lovrRetain(shape);

  if (shape->collider) {
    lovrColliderRemoveShape(shape->collider, shape);
  }

  shape->collider = collider;
  dGeomSetBody(shape->id, collider->body);
  dSpaceID newSpace = collider->world->space;
  dSpaceAdd(newSpace, shape->id);
}

void lovrColliderRemoveShape(Collider* collider, Shape* shape) {
  if (shape->collider == collider) {
    dSpaceRemove(collider->world->space, shape->id);
    dGeomSetBody(shape->id, 0);
    shape->collider = NULL;
    lovrRelease(shape, lovrShapeDestroy);
  }
}

Shape** lovrColliderGetShapes(Collider* collider, size_t* count) {
  arr_clear(&collider->shapes);
  for (dGeomID geom = dBodyGetFirstGeom(collider->body); geom; geom = dBodyGetNextGeom(geom)) {
    Shape* shape = dGeomGetData(geom);
    if (shape) {
      arr_push(&collider->shapes, shape);
    }
  }
  *count = collider->shapes.length;
  return collider->shapes.data;
}

Joint** lovrColliderGetJoints(Collider* collider, size_t* count) {
  arr_clear(&collider->joints);
  int jointCount = dBodyGetNumJoints(collider->body);
  for (int i = 0; i < jointCount; i++) {
    Joint* joint = dJointGetData(dBodyGetJoint(collider->body, i));
    if (joint) {
      arr_push(&collider->joints, joint);
    }
  }
  *count = collider->joints.length;
  return collider->joints.data;
}

void* lovrColliderGetUserData(Collider* collider) {
  return collider->userdata;
}

void lovrColliderSetUserData(Collider* collider, void* data) {
  collider->userdata = data;
}

const char* lovrColliderGetTag(Collider* collider) {
  return lovrWorldGetTagName(collider->world, collider->tag);
}

bool lovrColliderSetTag(Collider* collider, const char* tag) {
  if (!tag) {
    collider->tag = NO_TAG;
    return true;
  }

  collider->tag = findTag(collider->world, tag);
  return collider->tag != NO_TAG;
}

float lovrColliderGetFriction(Collider* collider) {
  return collider->friction;
}

void lovrColliderSetFriction(Collider* collider, float friction) {
  collider->friction = friction;
}

float lovrColliderGetRestitution(Collider* collider) {
  return collider->restitution;
}

void lovrColliderSetRestitution(Collider* collider, float restitution) {
  collider->restitution = restitution;
}

bool lovrColliderIsKinematic(Collider* collider) {
  return dBodyIsKinematic(collider->body);
}

void lovrColliderSetKinematic(Collider* collider, bool kinematic) {
  if (kinematic) {
    dBodySetKinematic(collider->body);
  } else {
    dBodySetDynamic(collider->body);
  }
}

bool lovrColliderIsGravityIgnored(Collider* collider) {
  return !dBodyGetGravityMode(collider->body);
}

void lovrColliderSetGravityIgnored(Collider* collider, bool ignored) {
  dBodySetGravityMode(collider->body, !ignored);
}

bool lovrColliderIsSleepingAllowed(Collider* collider) {
  return dBodyGetAutoDisableFlag(collider->body);
}

void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed) {
  dBodySetAutoDisableFlag(collider->body, allowed);
}

bool lovrColliderIsAwake(Collider* collider) {
  return dBodyIsEnabled(collider->body);
}

void lovrColliderSetAwake(Collider* collider, bool awake) {
  if (awake) {
    dBodyEnable(collider->body);
  } else {
    dBodyDisable(collider->body);
  }
}

float lovrColliderGetMass(Collider* collider) {
  dMass m;
  dBodyGetMass(collider->body, &m);
  return m.mass;
}

void lovrColliderSetMass(Collider* collider, float mass) {
  dMass m;
  dBodyGetMass(collider->body, &m);
  dMassAdjust(&m, mass);
  dBodySetMass(collider->body, &m);
}

void lovrColliderGetMassData(Collider* collider, float* cx, float* cy, float* cz, float* mass, float inertia[6]) {
  dMass m;
  dBodyGetMass(collider->body, &m);
  *cx = m.c[0];
  *cy = m.c[1];
  *cz = m.c[2];
  *mass = m.mass;

  // Diagonal
  inertia[0] = m.I[0];
  inertia[1] = m.I[5];
  inertia[2] = m.I[10];

  // Lower triangular
  inertia[3] = m.I[4];
  inertia[4] = m.I[8];
  inertia[5] = m.I[9];
}

void lovrColliderSetMassData(Collider* collider, float cx, float cy, float cz, float mass, float inertia[]) {
  dMass m;
  dBodyGetMass(collider->body, &m);
  dMassSetParameters(&m, mass, cx, cy, cz, inertia[0], inertia[1], inertia[2], inertia[3], inertia[4], inertia[5]);
  dBodySetMass(collider->body, &m);
}

void lovrColliderGetPosition(Collider* collider, float* x, float* y, float* z) {
  const dReal* position = dBodyGetPosition(collider->body);
  *x = position[0];
  *y = position[1];
  *z = position[2];
}

void lovrColliderSetPosition(Collider* collider, float x, float y, float z) {
  dBodySetPosition(collider->body, x, y, z);
}

void lovrColliderGetOrientation(Collider* collider, quat orientation) {
  const dReal* q = dBodyGetQuaternion(collider->body);
  quat_set(orientation, q[1], q[2], q[3], q[0]);
}

void lovrColliderSetOrientation(Collider* collider, quat orientation) {
  dReal q[4] = { orientation[3], orientation[0], orientation[1], orientation[2] };
  dBodySetQuaternion(collider->body, q);
}

void lovrColliderGetLinearVelocity(Collider* collider, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetLinearVel(collider->body);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrColliderSetLinearVelocity(Collider* collider, float x, float y, float z) {
  dBodySetLinearVel(collider->body, x, y, z);
}

void lovrColliderGetAngularVelocity(Collider* collider, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetAngularVel(collider->body);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrColliderSetAngularVelocity(Collider* collider, float x, float y, float z) {
  dBodySetAngularVel(collider->body, x, y, z);
}

void lovrColliderGetLinearDamping(Collider* collider, float* damping, float* threshold) {
  *damping = dBodyGetLinearDamping(collider->body);
  *threshold = dBodyGetLinearDampingThreshold(collider->body);
}

void lovrColliderSetLinearDamping(Collider* collider, float damping, float threshold) {
  dBodySetLinearDamping(collider->body, damping);
  dBodySetLinearDampingThreshold(collider->body, threshold);
}

void lovrColliderGetAngularDamping(Collider* collider, float* damping, float* threshold) {
  *damping = dBodyGetAngularDamping(collider->body);
  *threshold = dBodyGetAngularDampingThreshold(collider->body);
}

void lovrColliderSetAngularDamping(Collider* collider, float damping, float threshold) {
  dBodySetAngularDamping(collider->body, damping);
  dBodySetAngularDampingThreshold(collider->body, threshold);
}

void lovrColliderApplyForce(Collider* collider, float x, float y, float z) {
  dBodyAddForce(collider->body, x, y, z);
}

void lovrColliderApplyForceAtPosition(Collider* collider, float x, float y, float z, float cx, float cy, float cz) {
  dBodyAddForceAtPos(collider->body, x, y, z, cx, cy, cz);
}

void lovrColliderApplyTorque(Collider* collider, float x, float y, float z) {
  dBodyAddTorque(collider->body, x, y, z);
}

void lovrColliderGetLocalCenter(Collider* collider, float* x, float* y, float* z) {
  dMass m;
  dBodyGetMass(collider->body, &m);
  *x = m.c[0];
  *y = m.c[1];
  *z = m.c[2];
}

void lovrColliderGetLocalPoint(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z) {
  dReal local[3];
  dBodyGetPosRelPoint(collider->body, wx, wy, wz, local);
  *x = local[0];
  *y = local[1];
  *z = local[2];
}

void lovrColliderGetWorldPoint(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz) {
  dReal world[3];
  dBodyGetRelPointPos(collider->body, x, y, z, world);
  *wx = world[0];
  *wy = world[1];
  *wz = world[2];
}

void lovrColliderGetLocalVector(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z) {
  dReal local[3];
  dBodyVectorFromWorld(collider->body, wx, wy, wz, local);
  *x = local[0];
  *y = local[1];
  *z = local[2];
}

void lovrColliderGetWorldVector(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz) {
  dReal world[3];
  dBodyVectorToWorld(collider->body, x, y, z, world);
  *wx = world[0];
  *wy = world[1];
  *wz = world[2];
}

void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float x, float y, float z, float* vx, float* vy, float* vz) {
  dReal velocity[3];
  dBodyGetRelPointVel(collider->body, x, y, z, velocity);
  *vx = velocity[0];
  *vy = velocity[1];
  *vz = velocity[2];
}

void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float wx, float wy, float wz, float* vx, float* vy, float* vz) {
  dReal velocity[3];
  dBodyGetPointVel(collider->body, wx, wy, wz, velocity);
  *vx = velocity[0];
  *vy = velocity[1];
  *vz = velocity[2];
}

void lovrColliderGetAABB(Collider* collider, float aabb[6]) {
  dGeomID shape = dBodyGetFirstGeom(collider->body);

  if (!shape) {
    memset(aabb, 0, 6 * sizeof(float));
    return;
  }

  dGeomGetAABB(shape, aabb);

  float otherAABB[6];
  while ((shape = dBodyGetNextGeom(shape)) != NULL) {
    dGeomGetAABB(shape, otherAABB);
    aabb[0] = MIN(aabb[0], otherAABB[0]);
    aabb[1] = MAX(aabb[1], otherAABB[1]);
    aabb[2] = MIN(aabb[2], otherAABB[2]);
    aabb[3] = MAX(aabb[3], otherAABB[3]);
    aabb[4] = MIN(aabb[4], otherAABB[4]);
    aabb[5] = MAX(aabb[5], otherAABB[5]);
  }
}

void lovrShapeDestroy(void* ref) {
  Shape* shape = ref;
  lovrShapeDestroyData(shape);
  free(shape);
}

void lovrShapeDestroyData(Shape* shape) {
  if (shape->id) {
    if (shape->type == SHAPE_MESH) {
      dTriMeshDataID dataID = dGeomTriMeshGetData(shape->id);
      dGeomTriMeshDataDestroy(dataID);
      free(shape->vertices);
      free(shape->indices);
    }
    dGeomDestroy(shape->id);
    shape->id = NULL;
  }
}

ShapeType lovrShapeGetType(Shape* shape) {
  return shape->type;
}

Collider* lovrShapeGetCollider(Shape* shape) {
  return shape->collider;
}

bool lovrShapeIsEnabled(Shape* shape) {
  return dGeomIsEnabled(shape->id);
}

void lovrShapeSetEnabled(Shape* shape, bool enabled) {
  if (enabled) {
    dGeomEnable(shape->id);
  } else {
    dGeomDisable(shape->id);
  }
}

bool lovrShapeIsSensor(Shape* shape) {
  return shape->sensor;
}

void lovrShapeSetSensor(Shape* shape, bool sensor) {
  shape->sensor = sensor;
}

void* lovrShapeGetUserData(Shape* shape) {
  return shape->userdata;
}

void lovrShapeSetUserData(Shape* shape, void* data) {
  shape->userdata = data;
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

void lovrShapeGetOrientation(Shape* shape, quat orientation) {
  dReal q[4];
  dGeomGetOffsetQuaternion(shape->id, q);
  quat_set(orientation, q[1], q[2], q[3], q[0]);
}

void lovrShapeSetOrientation(Shape* shape, quat orientation) {
  dReal q[4] = { orientation[3], orientation[0], orientation[1], orientation[2] };
  dGeomSetOffsetQuaternion(shape->id, q);
}

void lovrShapeGetMass(Shape* shape, float density, float* cx, float* cy, float* cz, float* mass, float inertia[6]) {
  dMass m;
  dMassSetZero(&m);
  switch (shape->type) {
    case SHAPE_SPHERE: {
      dMassSetSphere(&m, density, dGeomSphereGetRadius(shape->id));
      break;
    }

    case SHAPE_BOX: {
      dReal lengths[3];
      dGeomBoxGetLengths(shape->id, lengths);
      dMassSetBox(&m, density, lengths[0], lengths[1], lengths[2]);
      break;
    }

    case SHAPE_CAPSULE: {
      dReal radius, length;
      dGeomCapsuleGetParams(shape->id, &radius, &length);
      dMassSetCapsule(&m, density, 3, radius, length);
      break;
    }

    case SHAPE_CYLINDER: {
      dReal radius, length;
      dGeomCylinderGetParams(shape->id, &radius, &length);
      dMassSetCylinder(&m, density, 3, radius, length);
      break;
    }

    case SHAPE_MESH: {
      dMassSetTrimesh(&m, density, shape->id);
      dGeomSetPosition(shape->id, -m.c[0], -m.c[1], -m.c[2]);
      dMassTranslate(&m, -m.c[0], -m.c[1], -m.c[2]);
      break;
    }
  }

  const dReal* position = dGeomGetOffsetPosition(shape->id);
  dMassTranslate(&m, position[0], position[1], position[2]);
  const dReal* rotation = dGeomGetOffsetRotation(shape->id);
  dMassRotate(&m, rotation);

  *cx = m.c[0];
  *cy = m.c[1];
  *cz = m.c[2];
  *mass = m.mass;

  // Diagonal
  inertia[0] = m.I[0];
  inertia[1] = m.I[5];
  inertia[2] = m.I[10];

  // Lower triangular
  inertia[3] = m.I[4];
  inertia[4] = m.I[8];
  inertia[5] = m.I[9];
}

void lovrShapeGetAABB(Shape* shape, float aabb[6]) {
  dGeomGetAABB(shape->id, aabb);
}

SphereShape* lovrSphereShapeCreate(float radius) {
  SphereShape* sphere = calloc(1, sizeof(SphereShape));
  lovrAssert(sphere, "Out of memory");
  sphere->ref = 1;
  sphere->type = SHAPE_SPHERE;
  sphere->id = dCreateSphere(0, radius);
  dGeomSetData(sphere->id, sphere);
  return sphere;
}

float lovrSphereShapeGetRadius(SphereShape* sphere) {
  return dGeomSphereGetRadius(sphere->id);
}

void lovrSphereShapeSetRadius(SphereShape* sphere, float radius) {
  dGeomSphereSetRadius(sphere->id, radius);
}

BoxShape* lovrBoxShapeCreate(float x, float y, float z) {
  BoxShape* box = calloc(1, sizeof(BoxShape));
  lovrAssert(box, "Out of memory");
  box->ref = 1;
  box->type = SHAPE_BOX;
  box->id = dCreateBox(0, x, y, z);
  dGeomSetData(box->id, box);
  return box;
}

void lovrBoxShapeGetDimensions(BoxShape* box, float* x, float* y, float* z) {
  dReal dimensions[3];
  dGeomBoxGetLengths(box->id, dimensions);
  *x = dimensions[0];
  *y = dimensions[1];
  *z = dimensions[2];
}

void lovrBoxShapeSetDimensions(BoxShape* box, float x, float y, float z) {
  dGeomBoxSetLengths(box->id, x, y, z);
}

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length) {
  CapsuleShape* capsule = calloc(1, sizeof(CapsuleShape));
  lovrAssert(capsule, "Out of memory");
  capsule->ref = 1;
  capsule->type = SHAPE_CAPSULE;
  capsule->id = dCreateCapsule(0, radius, length);
  dGeomSetData(capsule->id, capsule);
  return capsule;
}

float lovrCapsuleShapeGetRadius(CapsuleShape* capsule) {
  dReal radius, length;
  dGeomCapsuleGetParams(capsule->id, &radius, &length);
  return radius;
}

void lovrCapsuleShapeSetRadius(CapsuleShape* capsule, float radius) {
  dGeomCapsuleSetParams(capsule->id, radius, lovrCapsuleShapeGetLength(capsule));
}

float lovrCapsuleShapeGetLength(CapsuleShape* capsule) {
  dReal radius, length;
  dGeomCapsuleGetParams(capsule->id, &radius, &length);
  return length;
}

void lovrCapsuleShapeSetLength(CapsuleShape* capsule, float length) {
  dGeomCapsuleSetParams(capsule->id, lovrCapsuleShapeGetRadius(capsule), length);
}

CylinderShape* lovrCylinderShapeCreate(float radius, float length) {
  CylinderShape* cylinder = calloc(1, sizeof(CylinderShape));
  lovrAssert(cylinder, "Out of memory");
  cylinder->ref = 1;
  cylinder->type = SHAPE_CYLINDER;
  cylinder->id = dCreateCylinder(0, radius, length);
  dGeomSetData(cylinder->id, cylinder);
  return cylinder;
}

float lovrCylinderShapeGetRadius(CylinderShape* cylinder) {
  dReal radius, length;
  dGeomCylinderGetParams(cylinder->id, &radius, &length);
  return radius;
}

void lovrCylinderShapeSetRadius(CylinderShape* cylinder, float radius) {
  dGeomCylinderSetParams(cylinder->id, radius, lovrCylinderShapeGetLength(cylinder));
}

float lovrCylinderShapeGetLength(CylinderShape* cylinder) {
  dReal radius, length;
  dGeomCylinderGetParams(cylinder->id, &radius, &length);
  return length;
}

void lovrCylinderShapeSetLength(CylinderShape* cylinder, float length) {
  dGeomCylinderSetParams(cylinder->id, lovrCylinderShapeGetRadius(cylinder), length);
}

MeshShape* lovrMeshShapeCreate(int vertexCount, float* vertices, int indexCount, dTriIndex* indices) {
  MeshShape* mesh = calloc(1, sizeof(MeshShape));
  lovrAssert(mesh, "Out of memory");
  mesh->ref = 1;
  dTriMeshDataID dataID = dGeomTriMeshDataCreate();
  dGeomTriMeshDataBuildSingle(dataID, vertices, 3 * sizeof(float), vertexCount, indices, indexCount, 3 * sizeof(dTriIndex));
  dGeomTriMeshDataPreprocess2(dataID, (1U << dTRIDATAPREPROCESS_BUILD_FACE_ANGLES), NULL);
  mesh->id = dCreateTriMesh(0, dataID, 0, 0, 0);
  mesh->type = SHAPE_MESH;
  mesh->vertices = vertices;
  mesh->indices = indices;
  dGeomSetData(mesh->id, mesh);
  return mesh;
}

void lovrJointDestroy(void* ref) {
  Joint* joint = ref;
  lovrJointDestroyData(joint);
  free(joint);
}

void lovrJointDestroyData(Joint* joint) {
  if (joint->id) {
    dJointDestroy(joint->id);
    joint->id = NULL;
  }
}

JointType lovrJointGetType(Joint* joint) {
  return joint->type;
}

void lovrJointGetColliders(Joint* joint, Collider** a, Collider** b) {
  dBodyID bodyA = dJointGetBody(joint->id, 0);
  dBodyID bodyB = dJointGetBody(joint->id, 1);

  if (bodyA) {
    *a = dBodyGetData(bodyA);
  }

  if (bodyB) {
    *b = dBodyGetData(bodyB);
  }
}

void* lovrJointGetUserData(Joint* joint) {
  return joint->userdata;
}

void lovrJointSetUserData(Joint* joint, void* data) {
  joint->userdata = data;
}

bool lovrJointIsEnabled(Joint* joint) {
  return dJointIsEnabled(joint->id);
}

void lovrJointSetEnabled(Joint* joint, bool enable) {
  if (enable) {
    dJointEnable(joint->id);
  } else {
    dJointDisable(joint->id);
  }
}

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float x, float y, float z) {
  lovrAssert(a->world == b->world, "Joint bodies must exist in same World");
  BallJoint* joint = calloc(1, sizeof(BallJoint));
  lovrAssert(joint, "Out of memory");
  joint->ref = 1;
  joint->type = JOINT_BALL;
  joint->id = dJointCreateBall(a->world->id, 0);
  dJointSetData(joint->id, joint);
  dJointAttach(joint->id, a->body, b->body);
  lovrBallJointSetAnchor(joint, x, y, z);
  lovrRetain(joint);
  return joint;
}

void lovrBallJointGetAnchors(BallJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2) {
  dReal anchor[3];
  dJointGetBallAnchor(joint->id, anchor);
  *x1 = anchor[0];
  *y1 = anchor[1];
  *z1 = anchor[2];
  dJointGetBallAnchor2(joint->id, anchor);
  *x2 = anchor[0];
  *y2 = anchor[1];
  *z2 = anchor[2];
}

void lovrBallJointSetAnchor(BallJoint* joint, float x, float y, float z) {
  dJointSetBallAnchor(joint->id, x, y, z);
}

float lovrBallJointGetResponseTime(Joint* joint) {
  return dJointGetBallParam(joint->id, dParamCFM);
}

void lovrBallJointSetResponseTime(Joint* joint, float responseTime) {
  dJointSetBallParam(joint->id, dParamCFM, responseTime);
}

float lovrBallJointGetTightness(Joint* joint) {
  return dJointGetBallParam(joint->id, dParamERP);
}

void lovrBallJointSetTightness(Joint* joint, float tightness) {
  dJointSetBallParam(joint->id, dParamERP, tightness);
}

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float x1, float y1, float z1, float x2, float y2, float z2) {
  lovrAssert(a->world == b->world, "Joint bodies must exist in same World");
  DistanceJoint* joint = calloc(1, sizeof(DistanceJoint));
  lovrAssert(joint, "Out of memory");
  joint->ref = 1;
  joint->type = JOINT_DISTANCE;
  joint->id = dJointCreateDBall(a->world->id, 0);
  dJointSetData(joint->id, joint);
  dJointAttach(joint->id, a->body, b->body);
  lovrDistanceJointSetAnchors(joint, x1, y1, z1, x2, y2, z2);
  lovrRetain(joint);
  return joint;
}

void lovrDistanceJointGetAnchors(DistanceJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2) {
  dReal anchor[3];
  dJointGetDBallAnchor1(joint->id, anchor);
  *x1 = anchor[0];
  *y1 = anchor[1];
  *z1 = anchor[2];
  dJointGetDBallAnchor2(joint->id, anchor);
  *x2 = anchor[0];
  *y2 = anchor[1];
  *z2 = anchor[2];
}

void lovrDistanceJointSetAnchors(DistanceJoint* joint, float x1, float y1, float z1, float x2, float y2, float z2) {
  dJointSetDBallAnchor1(joint->id, x1, y1, z1);
  dJointSetDBallAnchor2(joint->id, x2, y2, z2);
}

float lovrDistanceJointGetDistance(DistanceJoint* joint) {
  return dJointGetDBallDistance(joint->id);
}

void lovrDistanceJointSetDistance(DistanceJoint* joint, float distance) {
  dJointSetDBallDistance(joint->id, distance);
}

float lovrDistanceJointGetResponseTime(Joint* joint) {
  return dJointGetDBallParam(joint->id, dParamCFM);
}

void lovrDistanceJointSetResponseTime(Joint* joint, float responseTime) {
  dJointSetDBallParam(joint->id, dParamCFM, responseTime);
}

float lovrDistanceJointGetTightness(Joint* joint) {
  return dJointGetDBallParam(joint->id, dParamERP);
}

void lovrDistanceJointSetTightness(Joint* joint, float tightness) {
  dJointSetDBallParam(joint->id, dParamERP, tightness);
}

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float x, float y, float z, float ax, float ay, float az) {
  lovrAssert(a->world == b->world, "Joint bodies must exist in same World");
  HingeJoint* joint = calloc(1, sizeof(HingeJoint));
  lovrAssert(joint, "Out of memory");
  joint->ref = 1;
  joint->type = JOINT_HINGE;
  joint->id = dJointCreateHinge(a->world->id, 0);
  dJointSetData(joint->id, joint);
  dJointAttach(joint->id, a->body, b->body);
  lovrHingeJointSetAnchor(joint, x, y, z);
  lovrHingeJointSetAxis(joint, ax, ay, az);
  lovrRetain(joint);
  return joint;
}

void lovrHingeJointGetAnchors(HingeJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2) {
  dReal anchor[3];
  dJointGetHingeAnchor(joint->id, anchor);
  *x1 = anchor[0];
  *y1 = anchor[1];
  *z1 = anchor[2];
  dJointGetHingeAnchor2(joint->id, anchor);
  *x2 = anchor[0];
  *y2 = anchor[1];
  *z2 = anchor[2];
}

void lovrHingeJointSetAnchor(HingeJoint* joint, float x, float y, float z) {
  dJointSetHingeAnchor(joint->id, x, y, z);
}

void lovrHingeJointGetAxis(HingeJoint* joint, float* x, float* y, float* z) {
  dReal axis[3];
  dJointGetHingeAxis(joint->id, axis);
  *x = axis[0];
  *y = axis[1];
  *z = axis[2];
}

void lovrHingeJointSetAxis(HingeJoint* joint, float x, float y, float z) {
  dJointSetHingeAxis(joint->id, x, y, z);
}

float lovrHingeJointGetAngle(HingeJoint* joint) {
  return dJointGetHingeAngle(joint->id);
}

float lovrHingeJointGetLowerLimit(HingeJoint* joint) {
  return dJointGetHingeParam(joint->id, dParamLoStop);
}

void lovrHingeJointSetLowerLimit(HingeJoint* joint, float limit) {
  dJointSetHingeParam(joint->id, dParamLoStop, limit);
}

float lovrHingeJointGetUpperLimit(HingeJoint* joint) {
  return dJointGetHingeParam(joint->id, dParamHiStop);
}

void lovrHingeJointSetUpperLimit(HingeJoint* joint, float limit) {
  dJointSetHingeParam(joint->id, dParamHiStop, limit);
}

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float ax, float ay, float az) {
  lovrAssert(a->world == b->world, "Joint bodies must exist in the same world");
  SliderJoint* joint = calloc(1, sizeof(SliderJoint));
  lovrAssert(joint, "Out of memory");
  joint->ref = 1;
  joint->type = JOINT_SLIDER;
  joint->id = dJointCreateSlider(a->world->id, 0);
  dJointSetData(joint->id, joint);
  dJointAttach(joint->id, a->body, b->body);
  lovrSliderJointSetAxis(joint, ax, ay, az);
  lovrRetain(joint);
  return joint;
}

void lovrSliderJointGetAxis(SliderJoint* joint, float* x, float* y, float* z) {
  dReal axis[3];
  dJointGetSliderAxis(joint->id, axis);
  *x = axis[0];
  *y = axis[1];
  *z = axis[2];
}

void lovrSliderJointSetAxis(SliderJoint* joint, float x, float y, float z) {
  dJointSetSliderAxis(joint->id, x, y, z);
}

float lovrSliderJointGetPosition(SliderJoint* joint) {
  return dJointGetSliderPosition(joint->id);
}

float lovrSliderJointGetLowerLimit(SliderJoint* joint) {
  return dJointGetSliderParam(joint->id, dParamLoStop);
}

void lovrSliderJointSetLowerLimit(SliderJoint* joint, float limit) {
  dJointSetSliderParam(joint->id, dParamLoStop, limit);
}

float lovrSliderJointGetUpperLimit(SliderJoint* joint) {
  return dJointGetSliderParam(joint->id, dParamHiStop);
}

void lovrSliderJointSetUpperLimit(SliderJoint* joint, float limit) {
  dJointSetSliderParam(joint->id, dParamHiStop, limit);
}
