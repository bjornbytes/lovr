#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <threads.h>
#include <joltc.h>

struct World {
  uint32_t ref;
  JPH_PhysicsSystem* system;
  JPH_BodyInterface* bodies;
  JPH_BodyActivationListener* activationListener;
  JPH_ObjectLayerPairFilter* objectLayerPairFilter;
  Collider* colliders;
  Collider** activeColliders;
  uint32_t activeColliderCount;
  Joint* joints;
  uint32_t jointCount;
  float defaultLinearDamping;
  float defaultAngularDamping;
  bool defaultIsSleepingAllowed;
  int collisionSteps;
  float time;
  float tickRate;
  uint32_t tickLimit;
  uint32_t tagCount;
  uint32_t staticTagMask;
  uint32_t tagLookup[MAX_TAGS];
  char* tags[MAX_TAGS];
  mtx_t lock;
};

struct Collider {
  uint32_t ref;
  JPH_BodyID id;
  JPH_Body* body;
  Collider* prev;
  Collider* next;
  World* world;
  Shape* shape;
  Joint* joints;
  uint32_t tag;
  uint32_t activeIndex;
  float lastPosition[4];
  float lastOrientation[4];
};

struct Shape {
  uint32_t ref;
  ShapeType type;
  JPH_Shape* handle;
};

typedef struct {
  Joint* prev;
  Joint* next;
} JointNode;

struct Joint {
  uint32_t ref;
  JointType type;
  JPH_Constraint* constraint;
  JointNode a, b, world;
};

static thread_local struct {
  JPH_BroadPhaseLayerFilter* broadPhaseLayerFilter;
  JPH_ObjectLayerFilter* objectLayerFilter;
} thread;

static struct {
  bool initialized;
} state;

#define vec3_toJolt(v) &(JPH_Vec3) { v[0], v[1], v[2] }
#define vec3_fromJolt(v, j) vec3_set(v, (j)->x, (j)->y, (j)->z)
#define quat_toJolt(q) &(JPH_Quat) { q[0], q[1], q[2], q[3] }
#define quat_fromJolt(q, j) quat_set(q, (j)->x, (j)->y, (j)->z, (j)->w)

static uint32_t findTag(World* world, const char* name, size_t length) {
  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint32_t i = 0; i < world->tagCount; i++) {
    if (world->tagLookup[i] == hash) {
      return i;
    }
  }
  return ~0u;
}

static Shape* subshapeToShape(Collider* collider, JPH_SubShapeID id) {
  if (collider->shape->type == SHAPE_COMPOUND) {
    JPH_SubShapeID remainder;
    uint32_t index = JPH_CompoundShape_GetSubShapeIndexFromID((JPH_CompoundShape*) collider->shape->handle, id, &remainder);
    return lovrCompoundShapeGetChild(collider->shape, index);
  } else {
    return collider->shape;
  }
}

static JPH_Bool32 broadPhaseLayerFilter(void* mask, JPH_BroadPhaseLayer layer) {
  return ((uintptr_t) mask & (1 << layer)) != 0;
}

static JPH_BroadPhaseLayerFilter* getBroadPhaseLayerFilter(World* world, uint32_t tagMask) {
  if (!thread.broadPhaseLayerFilter) {
    thread.broadPhaseLayerFilter = JPH_BroadPhaseLayerFilter_Create();
  }

  uint32_t layerMask = 0;
  if (~world->staticTagMask & tagMask) layerMask |= 0x1;
  if ( world->staticTagMask & tagMask) layerMask |= 0x3;

  JPH_BroadPhaseLayerFilter_SetProcs(thread.broadPhaseLayerFilter, (JPH_BroadPhaseLayerFilter_Procs) {
    .ShouldCollide = broadPhaseLayerFilter
  }, (void*) (uintptr_t) layerMask);

  return thread.broadPhaseLayerFilter;
}

static JPH_Bool32 objectLayerFilter(void* mask, JPH_ObjectLayer layer) {
  return ((uintptr_t) mask & (1 << layer)) != 0;
}

static JPH_ObjectLayerFilter* getObjectLayerFilter(World* world, uint32_t tagMask) {
  if (!thread.objectLayerFilter) {
    thread.objectLayerFilter = JPH_ObjectLayerFilter_Create();
  }

  JPH_ObjectLayerFilter_SetProcs(thread.objectLayerFilter, (JPH_ObjectLayerFilter_Procs) {
    .ShouldCollide = objectLayerFilter
  }, (void*) (uintptr_t) tagMask);

  return thread.objectLayerFilter;
}

static void onAwake(void* arg, JPH_BodyID id, uint64_t userData) {
  World* world = arg;
  mtx_lock(&world->lock);
  Collider* collider = (Collider*) (uintptr_t) userData;
  collider->activeIndex = world->activeColliderCount++;
  world->activeColliders[collider->activeIndex] = collider;
  mtx_unlock(&world->lock);
}

static void onSleep(void* arg, JPH_BodyID id, uint64_t userData) {
  World* world = arg;
  mtx_lock(&world->lock);
  Collider* collider = (Collider*) (uintptr_t) userData;
  if (collider->activeIndex != world->activeColliderCount - 1) {
    Collider* lastCollider = world->activeColliders[world->activeColliderCount - 1];
    world->activeColliders[collider->activeIndex] = lastCollider;
    lastCollider->activeIndex = collider->activeIndex;
  }
  world->activeColliderCount--;
  collider->activeIndex = ~0u;
  mtx_unlock(&world->lock);
}

bool lovrPhysicsInit(void) {
  if (state.initialized) return false;
  JPH_Init(32 * 1024 * 1024);
  return state.initialized = true;
}

void lovrPhysicsDestroy(void) {
  if (!state.initialized) return;
  JPH_Shutdown();
  state.initialized = false;
}

World* lovrWorldCreate(WorldInfo* info) {
  World* world = lovrCalloc(sizeof(World));

  world->ref = 1;
  world->collisionSteps = 1;
  world->defaultLinearDamping = .05f;
  world->defaultAngularDamping = .05f;
  world->defaultIsSleepingAllowed = info->allowSleep;
  mtx_init(&world->lock, mtx_plain);

  world->tagCount = info->tagCount;
  world->staticTagMask = info->staticTagMask;
  for (uint32_t i = 0; i < world->tagCount; i++) {
    size_t length = strlen(info->tags[i]);
    world->tags[i] = lovrMalloc(length + 1);
    memcpy(world->tags[i], info->tags[i], length + 1);
    world->tagLookup[i] = (uint32_t) hash64(info->tags[i], length);
  }

  uint32_t broadPhaseLayerCount = world->staticTagMask ? 2 : 1;
  uint32_t objectLayerCount = info->tagCount + 1;
  JPH_BroadPhaseLayerInterface* broadPhaseLayerInterface = JPH_BroadPhaseLayerInterfaceTable_Create(objectLayerCount, broadPhaseLayerCount);
  world->objectLayerPairFilter = JPH_ObjectLayerPairFilterTable_Create(objectLayerCount);

  // Each tag gets its own object layer, last object layer is for untagged colliders
  for (uint32_t i = 0; i < objectLayerCount; i++) {
    bool isStatic = world->staticTagMask & (1 << i);
    JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(broadPhaseLayerInterface, i, isStatic);

    // Static tags never collide with other static tags
    for (uint32_t j = i; j < objectLayerCount; j++) {
      if (isStatic && world->staticTagMask & (1 << j)) {
        JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, i, j);
      } else {
        JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, i, j);
      }
    }
  }

  JPH_ObjectVsBroadPhaseLayerFilter* broadPhaseLayerFilter = JPH_ObjectVsBroadPhaseLayerFilterTable_Create(
    broadPhaseLayerInterface, broadPhaseLayerCount,
    world->objectLayerPairFilter, objectLayerCount);

  JPH_PhysicsSystemSettings config = {
    .maxBodies = info->maxColliders,
    .maxBodyPairs = info->maxColliders * 2,
    .maxContactConstraints = info->maxColliders,
    .broadPhaseLayerInterface = broadPhaseLayerInterface,
    .objectLayerPairFilter = world->objectLayerPairFilter,
    .objectVsBroadPhaseLayerFilter = broadPhaseLayerFilter
  };

  world->system = JPH_PhysicsSystem_Create(&config);

  JPH_PhysicsSettings settings;
  JPH_PhysicsSystem_GetPhysicsSettings(world->system, &settings);
  settings.deterministicSimulation = info->deterministic;
  settings.allowSleeping = info->allowSleep;
  settings.baumgarte = CLAMP(info->stabilization, 0.f, 1.f);
  settings.penetrationSlop = MAX(info->maxPenetration, 0.f);
  settings.minVelocityForRestitution = MAX(info->minBounceVelocity, 0.f);
  settings.numVelocitySteps = MAX(settings.numVelocitySteps, 2);
  settings.numPositionSteps = MAX(settings.numPositionSteps, 1);
  JPH_PhysicsSystem_SetPhysicsSettings(world->system, &settings);

  world->bodies = info->threadSafe ?
    JPH_PhysicsSystem_GetBodyInterface(world->system) :
    JPH_PhysicsSystem_GetBodyInterfaceNoLock(world->system);

  world->tickRate = info->tickRate == 0 ? 0.f : 1.f / info->tickRate;
  world->tickLimit = info->tickLimit;

  if (world->tickRate > 0.f) {
    world->activeColliders = lovrMalloc(info->maxColliders * sizeof(Collider*));
    world->activationListener = JPH_BodyActivationListener_Create();

    JPH_BodyActivationListener_SetProcs(world->activationListener, (JPH_BodyActivationListener_Procs) {
      .OnBodyActivated = onAwake,
      .OnBodyDeactivated = onSleep
    }, world);

    JPH_PhysicsSystem_SetBodyActivationListener(world->system, world->activationListener);
  }

  return world;
}

void lovrWorldDestroy(void* ref) {
  World* world = ref;
  lovrWorldDestroyData(world);
  for (uint32_t i = 0; i < world->tagCount; i++) {
    lovrFree(world->tags[i]);
  }
  mtx_destroy(&world->lock);
  lovrFree(world->activeColliders);
  lovrFree(world);
}

void lovrWorldDestroyData(World* world) {
  while (world->colliders) {
    Collider* collider = world->colliders;
    Collider* next = collider->next;
    lovrColliderDestroyData(collider);
    world->colliders = next;
  }
  JPH_PhysicsSystem_Destroy(world->system);
  JPH_BodyActivationListener_Destroy(world->activationListener);
}

char** lovrWorldGetTags(World* world, uint32_t* count) {
  return world->tags;
}

uint32_t lovrWorldGetTagMask(World* world, const char* string, size_t length) {
  uint32_t accept = 0;
  uint32_t ignore = 0;

  while (length > 0) {
    if (*string == ' ') {
      string++;
      length--;
      continue;
    }

    bool invert = *string == '~';
    const char* space = memchr(string, ' ', length);
    size_t span = space ? space - string : length;
    uint32_t tag = findTag(world, string + invert, span - invert);
    lovrCheck(tag != ~0u, "Unknown tag in filter '%s'", string);
    if (invert) ignore |= (1 << tag);
    else accept |= (1 << tag);
    span += !!space;
    string += span;
    length -= span;
  }

  return accept ? accept : ~ignore;
}

uint32_t lovrWorldGetColliderCount(World* world) {
  return JPH_PhysicsSystem_GetNumBodies(world->system);
}

uint32_t lovrWorldGetJointCount(World* world) {
  return world->jointCount; // Jolt doesn't expose this
}

Collider* lovrWorldGetColliders(World* world, Collider* collider) {
  return collider ? collider->next : world->colliders;
}

Joint* lovrWorldGetJoints(World* world, Joint* joint) {
  return joint ? joint->world.next : world->joints;
}

void lovrWorldGetGravity(World* world, float gravity[3]) {
  JPH_Vec3 g;
  JPH_PhysicsSystem_GetGravity(world->system, &g);
  vec3_fromJolt(gravity, &g);
}

void lovrWorldSetGravity(World* world, float gravity[3]) {
  JPH_PhysicsSystem_SetGravity(world->system, vec3_toJolt(gravity));
}

void lovrWorldUpdate(World* world, float dt) {
  if (world->tickRate == 0.f) {
    JPH_PhysicsSystem_Step(world->system, dt, 1);
    return;
  }

  world->time += dt;

  uint32_t tick = 0;
  uint32_t lastTick = world->tickLimit - 1;

  while (world->time >= world->tickRate && tick <= lastTick) {
    world->time -= world->tickRate;

    if (world->time < world->tickRate || tick == lastTick) {
      for (uint32_t i = 0; i < world->activeColliderCount; i++) {
        Collider* collider = world->activeColliders[i];

        JPH_RVec3 position;
        JPH_Body_GetPosition(collider->body, &position);
        vec3_fromJolt(collider->lastPosition, &position);

        JPH_Quat orientation;
        JPH_Body_GetRotation(collider->body, &orientation);
        quat_fromJolt(collider->lastOrientation, &orientation);
      }
    }

    JPH_PhysicsSystem_Step(world->system, world->tickRate, 1);
    tick++;
  }
}

typedef struct {
  World* world;
  float* start;
  float* direction;
  CastCallback* callback;
  void* userdata;
} CastContext;

static float raycastCallback(void* arg, JPH_RayCastResult* result) {
  CastResult hit;
  CastContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodies, result->bodyID);
  hit.shape = subshapeToShape(hit.collider, result->subShapeID2);
  vec3_init(hit.position, ctx->direction);
  vec3_scale(hit.position, result->fraction);
  vec3_add(hit.position, ctx->start);
  JPH_Vec3 normal;
  JPH_Body_GetWorldSpaceSurfaceNormal(hit.collider->body, result->subShapeID2, vec3_toJolt(hit.position), &normal);
  vec3_fromJolt(hit.normal, &normal);
  hit.fraction = result->fraction;
  return ctx->callback(ctx->userdata, &hit);
}

bool lovrWorldRaycast(World* world, float start[3], float end[3], uint32_t filter, CastCallback* callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);

  float direction[3];
  vec3_init(direction, end);
  vec3_sub(direction, start);

  JPH_RVec3* origin = vec3_toJolt(start);
  JPH_Vec3* dir = vec3_toJolt(direction);

  CastContext context = {
    .world = world,
    .start = start,
    .direction = direction,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_NarrowPhaseQuery_CastRay2(query, origin, dir, raycastCallback, &context, layerFilter, tagFilter, NULL);
}

static float shapecastCallback(void* arg, JPH_ShapeCastResult* result) {
  CastResult hit;
  CastContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodies, result->bodyID2);
  hit.shape = subshapeToShape(hit.collider, result->subShapeID2);
  vec3_fromJolt(hit.position, &result->contactPointOn2);
  JPH_Vec3 normal;
  JPH_Body_GetWorldSpaceSurfaceNormal(hit.collider->body, result->subShapeID2, &result->contactPointOn2, &normal);
  vec3_fromJolt(hit.normal, &normal);
  hit.fraction = result->fraction;
  return ctx->callback(ctx->userdata, &hit);
}

bool lovrWorldShapecast(World* world, Shape* shape, float pose[7], float scale, float end[3], uint32_t filter, CastCallback callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);

  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);

  JPH_RMatrix4x4 transform;
  mat4_fromPose(&transform.m11, pose, pose + 3);
  mat4_translate(&transform.m11, centerOfMass.x, centerOfMass.y, centerOfMass.z);
  mat4_scale(&transform.m11, scale, scale, scale); // TODO does this work, or should we use scale arg?

  float direction[3];
  vec3_init(direction, end);
  vec3_sub(direction, pose);
  JPH_Vec3* dir = vec3_toJolt(direction);
  JPH_RVec3 offset = { 0.f, 0.f, 0.f };

  CastContext context = {
    .world = world,
    .start = pose,
    .direction = direction,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_NarrowPhaseQuery_CastShape(query, shape->handle, &transform, dir, &offset, shapecastCallback, &context, layerFilter, tagFilter, NULL);
}

typedef struct {
  World* world;
  CollideCallback* callback;
  void* userdata;
} CollideContext;

static float collideCallback(void* arg, JPH_CollideShapeResult* result) {
  CastResult hit;
  CollideContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodies, result->bodyID2);
  hit.shape = subshapeToShape(hit.collider, result->subShapeID2);
  vec3_fromJolt(hit.position, &result->contactPointOn2);
  vec3_fromJolt(hit.normal, &result->penetrationAxis);
  vec3_scale(vec3_normalize(hit.normal), result->penetrationDepth);
  return ctx->callback(ctx->userdata, &hit);
}

bool lovrWorldCollideShape(World* world, Shape* shape, float pose[7], float scale, uint32_t filter, CollideCallback* callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);

  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);

  JPH_RMatrix4x4 transform;
  mat4_fromPose(&transform.m11, pose, pose + 3);
  mat4_translate(&transform.m11, centerOfMass.x, centerOfMass.y, centerOfMass.z);

  JPH_Vec3 scale3 = { scale, scale, scale };
  JPH_RVec3 offset = { 0.f, 0.f, 0.f };

  CollideContext context = {
    .world = world,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_NarrowPhaseQuery_CollideShape(query, shape->handle, &scale3, &transform, &offset, collideCallback, &context, layerFilter, tagFilter, NULL);
}

typedef struct {
  World* world;
  QueryCallback* callback;
  void* userdata;
} QueryContext;

static void queryCallback(void* arg, JPH_BodyID id) {
  QueryContext* ctx = arg;
  Collider* collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodies, id);
  ctx->callback(ctx->userdata, collider);
}

bool lovrWorldQueryBox(World* world, float position[3], float size[3], uint32_t filter, QueryCallback* callback, void* userdata) {
  const JPH_BroadPhaseQuery* query = JPH_PhysicsSystem_GetBroadPhaseQuery(world->system);

  JPH_AABox box;
  box.min.x = position[0] - size[0] * .5f;
  box.min.y = position[1] - size[1] * .5f;
  box.min.z = position[2] - size[2] * .5f;
  box.max.x = position[0] + size[0] * .5f;
  box.max.y = position[1] + size[1] * .5f;
  box.max.z = position[2] + size[2] * .5f;

  QueryContext context = {
    .world = world,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_BroadPhaseQuery_CollideAABox(query, &box, queryCallback, &context, layerFilter, tagFilter);
}

bool lovrWorldQuerySphere(World* world, float position[3], float radius, uint32_t filter, QueryCallback* callback, void* userdata) {
  const JPH_BroadPhaseQuery* query = JPH_PhysicsSystem_GetBroadPhaseQuery(world->system);

  QueryContext context = {
    .world = world,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_BroadPhaseQuery_CollideSphere(query, vec3_toJolt(position), radius, queryCallback, &context, layerFilter, tagFilter);
}

void lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1, strlen(tag1));
  uint32_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != ~0u, "Unknown tag '%s'", tag1);
  lovrCheck(j != ~0u, "Unknown tag '%s'", tag2);
  JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, i, j);
}

void lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1, strlen(tag1));
  uint32_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != ~0u, "Unknown tag '%s'", tag1);
  lovrCheck(j != ~0u, "Unknown tag '%s'", tag2);
  JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, i, j);
}

bool lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2) {
  if (!tag1 || !tag2) return true;
  uint32_t i = findTag(world, tag1, strlen(tag1));
  uint32_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != ~0u, "Unknown tag '%s'", tag1);
  lovrCheck(j != ~0u, "Unknown tag '%s'", tag2);
  return JPH_ObjectLayerPairFilterTable_ShouldCollide(world->objectLayerPairFilter, i, j);
}

// Deprecated
int lovrWorldGetStepCount(World* world) { return world->collisionSteps; }
void lovrWorldSetStepCount(World* world, int iterations) { world->collisionSteps = iterations;}
float lovrWorldGetResponseTime(World* world) {}
void lovrWorldSetResponseTime(World* world, float responseTime) {}
float lovrWorldGetTightness(World* world) {}
void lovrWorldSetTightness(World* world, float tightness) {}
bool lovrWorldIsSleepingAllowed(World* world) { return world->defaultIsSleepingAllowed; }
void lovrWorldSetSleepingAllowed(World* world, bool allowed) { world->defaultIsSleepingAllowed = allowed; }
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold) { *damping = world->defaultLinearDamping, *threshold = 0.f; }
void lovrWorldSetLinearDamping(World* world, float damping, float threshold) { world->defaultLinearDamping = damping; }
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold) { *damping = world->defaultAngularDamping, *threshold = 0.f; }
void lovrWorldSetAngularDamping(World* world, float damping, float threshold) { world->defaultAngularDamping = damping; }

// Collider

static void changeCenterOfMass(Collider* collider, const JPH_Shape* oldShape, const JPH_Shape* newShape) {
  if (collider->joints) {
    JPH_Vec3 oldCenter;
    JPH_Vec3 newCenter;
    JPH_Shape_GetCenterOfMass(oldShape, &oldCenter);
    JPH_Shape_GetCenterOfMass(newShape, &newCenter);
    JPH_Vec3 delta = { newCenter.x - oldCenter.x, newCenter.y - oldCenter.y, newCenter.z - oldCenter.z };
    for (Joint* joint = collider->joints; joint; joint = lovrJointGetNext(joint, collider)) {
      JPH_Constraint_NotifyShapeChanged(joint->constraint, collider->id, &delta);
    }
  }
}

Collider* lovrColliderCreate(World* world, float position[3], Shape* shape) {
  uint32_t count = JPH_PhysicsSystem_GetNumBodies(world->system);
  uint32_t limit = JPH_PhysicsSystem_GetMaxBodies(world->system);
  lovrCheck(count < limit, "Too many colliders!");

  Collider* collider = lovrCalloc(sizeof(Collider));
  collider->ref = 1;
  collider->world = world;
  collider->shape = shape;
  collider->tag = ~0u;

  JPH_RVec3* p = vec3_toJolt(position);
  JPH_Quat q = { 0.f, 0.f, 0.f, 1.f };
  JPH_MotionType type = JPH_Shape_MustBeStatic(collider->shape->handle) ? JPH_MotionType_Static : JPH_MotionType_Dynamic;
  JPH_ObjectLayer objectLayer = world->tagCount;
  JPH_BodyCreationSettings* settings = JPH_BodyCreationSettings_Create3(collider->shape->handle, p, &q, type, objectLayer);
  collider->body = JPH_BodyInterface_CreateBody(world->bodies, settings);
  collider->id = JPH_Body_GetID(collider->body);
  JPH_BodyCreationSettings_Destroy(settings);

  JPH_BodyInterface_SetUserData(world->bodies, collider->id, (uint64_t) collider);
  JPH_BodyInterface_AddBody(world->bodies, collider->id, JPH_Activation_Activate);

  vec3_init(collider->lastPosition, position);
  quat_identity(collider->lastOrientation);

  if (type == JPH_MotionType_Dynamic) {
    lovrColliderSetLinearDamping(collider, world->defaultLinearDamping, 0.f);
    lovrColliderSetAngularDamping(collider, world->defaultAngularDamping, 0.f);
    lovrColliderSetSleepingAllowed(collider, world->defaultIsSleepingAllowed);
  }

  if (world->colliders) {
    collider->next = world->colliders;
    collider->next->prev = collider;
  }

  world->colliders = collider;

  lovrRetain(collider->shape);
  lovrRetain(collider);
  return collider;
}

void lovrColliderDestroy(void* ref) {
  Collider* collider = ref;
  lovrColliderDestroyData(collider);
  lovrFree(collider);
}

void lovrColliderDestroyData(Collider* collider) {
  if (!collider->body) {
    return;
  }

  lovrRelease(collider->shape, lovrShapeDestroy);

  Joint* joint = collider->joints;

  while (joint) {
    Joint* next = lovrJointGetNext(joint, collider);
    lovrJointDestroyData(joint);
    joint = next;
  }

  World* world = collider->world;
  JPH_BodyInterface_RemoveBody(world->bodies, collider->id);
  JPH_BodyInterface_DestroyBody(world->bodies, collider->id);
  collider->body = NULL;

  if (collider->next) collider->next->prev = collider->prev;
  if (collider->prev) collider->prev->next = collider->next;
  if (world->colliders == collider) world->colliders = collider->next;
  collider->next = collider->prev = NULL;

  // If the Collider is destroyed, the world lets go of its reference to this Collider
  lovrRelease(collider, lovrColliderDestroy);
}

bool lovrColliderIsDestroyed(Collider* collider) {
  return !collider->body;
}

bool lovrColliderIsEnabled(Collider* collider) {
  return JPH_BodyInterface_IsAdded(collider->world->bodies, collider->id);
}

void lovrColliderSetEnabled(Collider* collider, bool enable) {
  if (enable && !lovrColliderIsEnabled(collider)) {
    JPH_BodyInterface_AddBody(collider->world->bodies, collider->id, JPH_Activation_DontActivate);
  } else if (!enable && lovrColliderIsEnabled(collider)) {
    JPH_BodyInterface_RemoveBody(collider->world->bodies, collider->id);
  }
}

World* lovrColliderGetWorld(Collider* collider) {
  return collider->world;
}

Joint* lovrColliderGetJoints(Collider* collider, Joint* joint) {
  return joint ? lovrJointGetNext(joint, collider) : collider->joints;
}

Shape* lovrColliderGetShape(Collider* collider) {
  return collider->shape;
}

void lovrColliderSetShape(Collider* collider, Shape* shape) {
  if (shape == collider->shape) {
    return;
  }

  const JPH_Shape* oldShape = JPH_BodyInterface_GetShape(collider->world->bodies, collider->id);
  changeCenterOfMass(collider, oldShape, shape->handle);

  lovrRelease(collider->shape, lovrShapeDestroy);
  collider->shape = shape;
  lovrRetain(shape);

  bool updateMass = true;
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    JPH_BodyInterface_SetMotionType(collider->world->bodies, collider->id, JPH_MotionType_Static, JPH_Activation_DontActivate);
    updateMass = false;
  }

  JPH_BodyInterface_SetShape(collider->world->bodies, collider->id, shape->handle, updateMass, JPH_Activation_Activate);
}

const char* lovrColliderGetTag(Collider* collider) {
  return collider->tag == ~0u ? NULL : collider->world->tags[collider->tag];
}

void lovrColliderSetTag(Collider* collider, const char* tag) {
  collider->tag = tag ? findTag(collider->world, tag, strlen(tag)) : ~0u;
  lovrCheck(!tag || collider->tag != ~0u, "Unknown tag '%s'", tag);
  JPH_ObjectLayer objectLayer = collider->tag == ~0u ? collider->world->tagCount : collider->tag;
  JPH_BodyInterface_SetObjectLayer(collider->world->bodies, collider->id, objectLayer);

  if (collider->tag != ~0u && collider->world->staticTagMask & (1 << collider->tag)) {
    JPH_BodyInterface_SetMotionType(collider->world->bodies, collider->id, JPH_MotionType_Static, JPH_Activation_DontActivate);
  }
}

float lovrColliderGetFriction(Collider* collider) {
  return JPH_BodyInterface_GetFriction(collider->world->bodies, collider->id);
}

void lovrColliderSetFriction(Collider* collider, float friction) {
  JPH_BodyInterface_SetFriction(collider->world->bodies, collider->id, friction);
}

float lovrColliderGetRestitution(Collider* collider) {
  return JPH_BodyInterface_GetRestitution(collider->world->bodies, collider->id);
}

void lovrColliderSetRestitution(Collider* collider, float restitution) {
  JPH_BodyInterface_SetRestitution(collider->world->bodies, collider->id, restitution);
}

bool lovrColliderIsKinematic(Collider* collider) {
  return JPH_BodyInterface_GetMotionType(collider->world->bodies, collider->id) != JPH_MotionType_Dynamic;
}

void lovrColliderSetKinematic(Collider* collider, bool kinematic) {
  if (collider->tag == ~0u || (collider->world->staticTagMask & (1 << collider->tag)) == 0) {
    JPH_MotionType motionType = kinematic ? JPH_MotionType_Kinematic : JPH_MotionType_Dynamic;
    JPH_BodyInterface_SetMotionType(collider->world->bodies, collider->id, motionType, JPH_Activation_DontActivate);
  }
}

bool lovrColliderIsSensor(Collider* collider) {
  return JPH_Body_IsSensor(collider->body);
}

void lovrColliderSetSensor(Collider* collider, bool sensor) {
  JPH_Body_SetIsSensor(collider->body, sensor);
}

bool lovrColliderIsContinuous(Collider* collider) {
  return JPH_BodyInterface_GetMotionQuality(collider->world->bodies, collider->id) == JPH_MotionQuality_LinearCast;
}

void lovrColliderSetContinuous(Collider* collider, bool continuous) {
  JPH_MotionQuality quality = continuous ? JPH_MotionQuality_LinearCast : JPH_MotionQuality_Discrete;
  return JPH_BodyInterface_SetMotionQuality(collider->world->bodies, collider->id, quality);
}

float lovrColliderGetGravityScale(Collider* collider) {
  return JPH_BodyInterface_GetGravityFactor(collider->world->bodies, collider->id);
}

void lovrColliderSetGravityScale(Collider* collider, float scale) {
  return JPH_BodyInterface_SetGravityFactor(collider->world->bodies, collider->id, scale);
}

bool lovrColliderIsSleepingAllowed(Collider* collider) {
  return JPH_Body_GetAllowSleeping(collider->body);
}

void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed) {
  JPH_Body_SetAllowSleeping(collider->body, allowed);
}

bool lovrColliderIsAwake(Collider* collider) {
  return JPH_BodyInterface_IsActive(collider->world->bodies, collider->id);
}

void lovrColliderSetAwake(Collider* collider, bool awake) {
  if (awake) {
    JPH_BodyInterface_ActivateBody(collider->world->bodies, collider->id);
  } else {
    JPH_BodyInterface_DeactivateBody(collider->world->bodies, collider->id);
  }
}

float lovrColliderGetMass(Collider* collider) {
  if (lovrColliderIsKinematic(collider)) {
    return 0.f;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inverse mass is locked to zero
  if (JPH_MotionProperties_GetAllowedDOFs(motion) & 0x7 == 0) {
    return 0.f;
  }

  return 1.f / JPH_MotionProperties_GetInverseMassUnchecked(motion);
}

void lovrColliderSetMass(Collider* collider, float* mass) {
  if (lovrColliderIsKinematic(collider)) {
    return;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inverse mass is locked to zero
  if (JPH_MotionProperties_GetAllowedDOFs(motion) & 0x7 == 0) {
    return;
  }

  if (mass) {
    lovrCheck(*mass > 0.f, "Mass must be positive");
    JPH_MotionProperties_SetInverseMass(motion, 1.f / *mass);
  } else {
    JPH_MotionProperties_SetInverseMass(motion, 1.f / lovrShapeGetMass(collider->shape));
  }
}

void lovrColliderGetInertia(Collider* collider, float diagonal[3], float rotation[4]) {
  if (lovrColliderIsKinematic(collider)) {
    vec3_set(diagonal, 0.f, 0.f, 0.f);
    quat_identity(rotation);
    return;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inertia is locked to zero
  if (JPH_MotionProperties_GetAllowedDOFs(motion) & 0x38 == 0) {
    vec3_set(diagonal, 0.f, 0.f, 0.f);
    quat_identity(rotation);
    return;
  }

  JPH_Vec3 idiagonal;
  JPH_MotionProperties_GetInverseInertiaDiagonal(motion, &idiagonal);
  vec3_set(diagonal, 1.f / idiagonal.x, 1.f / idiagonal.y, 1.f / idiagonal.z);

  JPH_Quat q;
  JPH_MotionProperties_GetInertiaRotation(motion, &q);
  quat_fromJolt(rotation, &q);
}

void lovrColliderSetInertia(Collider* collider, float diagonal[3], float rotation[4]) {
  if (lovrColliderIsKinematic(collider)) {
    return;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inverse inertia is locked to zero
  if (JPH_MotionProperties_GetAllowedDOFs(motion) & 0x38 == 0) {
    return;
  }

  if (diagonal) {
    JPH_Vec3 idiagonal = { 1.f / diagonal[0], 1.f / diagonal[1], 1.f / diagonal[2] };
    JPH_MotionProperties_SetInverseInertia(motion, &idiagonal, quat_toJolt(rotation));
  } else {
    JPH_MassProperties massProperties;
    JPH_Shape_GetMassProperties(JPH_BodyInterface_GetShape(collider->world->bodies, collider->id), &massProperties);
    float inverseMass = JPH_MotionProperties_GetInverseMassUnchecked(motion);
    if (inverseMass > 0.f) JPH_MassProperties_ScaleToMass(&massProperties, 1.f / inverseMass);
    JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);
    JPH_MotionProperties_SetMassProperties(motion, dofs, &massProperties);
  }
}

void lovrColliderGetCenterOfMass(Collider* collider, float center[3]) {
  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(JPH_BodyInterface_GetShape(collider->world->bodies, collider->id), &centerOfMass);
  vec3_fromJolt(center, &centerOfMass);
}

void lovrColliderSetCenterOfMass(Collider* collider, float center[3]) {
  JPH_Shape* oldShape = (JPH_Shape*) JPH_BodyInterface_GetShape(collider->world->bodies, collider->id);
  JPH_Shape* newShape;

  if (center) {
    JPH_Vec3 base;
    JPH_Shape_GetCenterOfMass(collider->shape->handle, &base);

    JPH_Vec3 offset = { center[0] - base.x, center[1] - base.y, center[2] - base.z };
    newShape = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, collider->shape->handle);
  } else {
    newShape = collider->shape->handle;
  }

  changeCenterOfMass(collider, oldShape, newShape);
  JPH_BodyInterface_SetShape(collider->world->bodies, collider->id, newShape, false, JPH_Activation_DontActivate);

  if (JPH_Shape_GetSubType(oldShape) == JPH_ShapeSubType_OffsetCenterOfMass) {
    JPH_Shape_Destroy(oldShape);
  }
}

void lovrColliderResetMassData(Collider* collider) {
  if (lovrColliderIsKinematic(collider)) {
    return;
  }

  JPH_Shape* shape = (JPH_Shape*) JPH_BodyInterface_GetShape(collider->world->bodies, collider->id);

  if (JPH_Shape_GetSubType(shape) == JPH_ShapeSubType_OffsetCenterOfMass) {
    JPH_Shape_Destroy(shape);
    changeCenterOfMass(collider, shape, collider->shape->handle);
    JPH_BodyInterface_SetShape(collider->world->bodies, collider->id, collider->shape->handle, false, JPH_Activation_DontActivate);
  }

  JPH_MassProperties mass;
  JPH_Shape_GetMassProperties(collider->shape->handle, &mass);

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);
  JPH_MotionProperties_SetMassProperties(motion, dofs, &mass);
}

void lovrColliderGetEnabledAxes(Collider* collider, bool translation[3], bool rotation[3]) {
  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);
  if (dofs & JPH_AllowedDOFs_TranslationX) translation[0] = true;
  if (dofs & JPH_AllowedDOFs_TranslationY) translation[1] = true;
  if (dofs & JPH_AllowedDOFs_TranslationZ) translation[2] = true;
  if (dofs & JPH_AllowedDOFs_RotationX) rotation[0] = true;
  if (dofs & JPH_AllowedDOFs_RotationY) rotation[1] = true;
  if (dofs & JPH_AllowedDOFs_RotationZ) rotation[2] = true;
}

void lovrColliderSetEnabledAxes(Collider* collider, bool translation[3], bool rotation[3]) {
  JPH_AllowedDOFs dofs = 0;

  if (translation[0]) dofs |= JPH_AllowedDOFs_TranslationX;
  if (translation[1]) dofs |= JPH_AllowedDOFs_TranslationY;
  if (translation[2]) dofs |= JPH_AllowedDOFs_TranslationZ;
  if (rotation[0]) dofs |= JPH_AllowedDOFs_RotationX;
  if (rotation[1]) dofs |= JPH_AllowedDOFs_RotationY;
  if (rotation[2]) dofs |= JPH_AllowedDOFs_RotationZ;

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_MassProperties mass;
  JPH_Shape_GetMassProperties(collider->shape->handle, &mass);
  JPH_MotionProperties_SetMassProperties(motion, dofs, &mass);
}

void lovrColliderGetPosition(Collider* collider, float position[3]) {
  JPH_RVec3 p;
  JPH_Body_GetPosition(collider->body, &p);
  vec3_fromJolt(position, &p);
  if (collider->world->tickRate > 0.f && lovrColliderIsAwake(collider)) {
    vec3_lerp(position, collider->lastPosition, 1.f - collider->world->time / collider->world->tickRate);
  }
}

void lovrColliderSetPosition(Collider* collider, float position[3]) {
  JPH_BodyInterface_SetPosition(collider->world->bodies, collider->id, vec3_toJolt(position), JPH_Activation_Activate);
  vec3_init(collider->lastPosition, position);
}

void lovrColliderGetOrientation(Collider* collider, float orientation[4]) {
  JPH_Quat q;
  JPH_Body_GetRotation(collider->body, &q);
  quat_fromJolt(orientation, &q);
  if (collider->world->tickRate > 0.f && lovrColliderIsAwake(collider)) {
    quat_slerp(orientation, collider->lastOrientation, 1.f - collider->world->time / collider->world->tickRate);
  }
}

void lovrColliderSetOrientation(Collider* collider, float orientation[4]) {
  JPH_BodyInterface_SetRotation(collider->world->bodies, collider->id, quat_toJolt(orientation), JPH_Activation_Activate);
  quat_init(collider->lastOrientation, orientation);
}

void lovrColliderGetRawPosition(Collider* collider, float position[3]) {
  JPH_RVec3 p;
  JPH_Body_GetPosition(collider->body, &p);
  vec3_fromJolt(position, &p);
}

void lovrColliderGetRawOrientation(Collider* collider, float orientation[4]) {
  JPH_Quat q;
  JPH_Body_GetRotation(collider->body, &q);
  quat_fromJolt(orientation, &q);
}

void lovrColliderGetLinearVelocity(Collider* collider, float velocity[3]) {
  JPH_Vec3 v;
  JPH_BodyInterface_GetLinearVelocity(collider->world->bodies, collider->id, &v);
  vec3_fromJolt(velocity, &v);
}

void lovrColliderSetLinearVelocity(Collider* collider, float velocity[3]) {
  JPH_BodyInterface_SetLinearVelocity(collider->world->bodies, collider->id, vec3_toJolt(velocity));
}

void lovrColliderGetAngularVelocity(Collider* collider, float velocity[3]) {
  JPH_Vec3 v;
  JPH_BodyInterface_GetAngularVelocity(collider->world->bodies, collider->id, &v);
  vec3_fromJolt(velocity, &v);
}

void lovrColliderSetAngularVelocity(Collider* collider, float velocity[3]) {
  JPH_BodyInterface_SetAngularVelocity(collider->world->bodies, collider->id, vec3_toJolt(velocity));
}

void lovrColliderGetLinearDamping(Collider* collider, float* damping, float* threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  *damping = JPH_MotionProperties_GetLinearDamping(properties);
  *threshold = 0.f;
}

void lovrColliderSetLinearDamping(Collider* collider, float damping, float threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetLinearDamping(properties, damping);
}

void lovrColliderGetAngularDamping(Collider* collider, float* damping, float* threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  *damping = JPH_MotionProperties_GetAngularDamping(properties);
  *threshold = 0.f;
}

void lovrColliderSetAngularDamping(Collider* collider, float damping, float threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetAngularDamping(properties, damping);
}

void lovrColliderApplyForce(Collider* collider, float force[3]) {
  JPH_BodyInterface_AddForce(collider->world->bodies, collider->id, vec3_toJolt(force));
}

void lovrColliderApplyForceAtPosition(Collider* collider, float force[3], float position[3]) {
  JPH_BodyInterface_AddForce2(collider->world->bodies, collider->id, vec3_toJolt(force), vec3_toJolt(position));
}

void lovrColliderApplyTorque(Collider* collider, float torque[3]) {
  JPH_BodyInterface_AddTorque(collider->world->bodies, collider->id, vec3_toJolt(torque));
}

void lovrColliderApplyLinearImpulse(Collider* collider, float impulse[3]) {
  JPH_BodyInterface_AddImpulse(collider->world->bodies, collider->id, vec3_toJolt(impulse));
}

void lovrColliderApplyLinearImpulseAtPosition(Collider* collider, float impulse[3], float position[3]) {
  JPH_BodyInterface_AddImpulse2(collider->world->bodies, collider->id, vec3_toJolt(impulse), vec3_toJolt(position));
}

void lovrColliderApplyAngularImpulse(Collider* collider, float impulse[3]) {
  JPH_BodyInterface_AddAngularImpulse(collider->world->bodies, collider->id, vec3_toJolt(impulse));
}

void lovrColliderGetLocalCenter(Collider* collider, float center[3]) {
  JPH_Vec3 v;
  JPH_Shape_GetCenterOfMass(JPH_BodyInterface_GetShape(collider->world->bodies, collider->id), &v);
  vec3_fromJolt(center, &v);
}

void lovrColliderGetWorldCenter(Collider* collider, float center[3]) {
  JPH_RVec3 v;
  JPH_Body_GetCenterOfMassPosition(collider->body, &v);
  vec3_fromJolt(center, &v);
}

void lovrColliderGetLocalPoint(Collider* collider, float world[3], float local[3]) {
  JPH_RMatrix4x4 transform;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  vec3_init(local, world);
  mat4_invert(&transform.m11);
  mat4_mulPoint(&transform.m11, local);
}

void lovrColliderGetWorldPoint(Collider* collider, float local[3], float world[3]) {
  JPH_RMatrix4x4 transform;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  vec3_init(world, local);
  mat4_mulPoint(&transform.m11, world);
}

void lovrColliderGetLocalVector(Collider* collider, float world[3], float local[3]) {
  JPH_RMatrix4x4 transform;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  vec3_init(local, world);
  mat4_invert(&transform.m11);
  mat4_mulDirection(&transform.m11, local);
}

void lovrColliderGetWorldVector(Collider* collider, float local[3], float world[3]) {
  JPH_RMatrix4x4 transform;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  vec3_init(world, local);
  mat4_mulDirection(&transform.m11, world);
}

void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float point[3], float velocity[3]) {
  float wx, wy, wz;
  lovrColliderGetWorldPoint(collider, point, velocity);
  lovrColliderGetLinearVelocityFromWorldPoint(collider, point, velocity);
}

void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float point[3], float velocity[3]) {
  JPH_RVec3 p = { point[0], point[1], point[2] };
  JPH_Vec3 v;
  JPH_BodyInterface_GetPointVelocity(collider->world->bodies, collider->id, &p, &v);
  vec3_fromJolt(velocity, &v);
}

void lovrColliderGetAABB(Collider* collider, float aabb[6]) {
  JPH_AABox box;
  JPH_Body_GetWorldSpaceBounds(collider->body, &box);
  aabb[0] = box.min.x;
  aabb[1] = box.max.x;
  aabb[2] = box.min.y;
  aabb[3] = box.max.y;
  aabb[4] = box.min.z;
  aabb[5] = box.max.z;
}

// Shapes

void lovrShapeDestroy(void* ref) {
  Shape* shape = ref;

  if (shape->type == SHAPE_COMPOUND) {
    uint32_t count = lovrCompoundShapeGetChildCount(shape);
    for (uint32_t i = 0; i < count; i++) {
      Shape* child = lovrCompoundShapeGetChild(shape, i);
      lovrRelease(child, lovrShapeDestroy);
    }
  }

  JPH_Shape_Destroy(shape->handle);
  lovrFree(shape);
}

ShapeType lovrShapeGetType(Shape* shape) {
  return shape->type;
}

float lovrShapeGetVolume(Shape* shape) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    return 0.f;
  } else {
    return JPH_Shape_GetVolume(shape->handle);
  }
}

float lovrShapeGetDensity(Shape* shape) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN || shape->type == SHAPE_COMPOUND) {
    return 0.f;
  } else {
    return JPH_ConvexShape_GetDensity((JPH_ConvexShape*) shape->handle);
  }
}

void lovrShapeSetDensity(Shape* shape, float density) {
  if (shape->type != SHAPE_MESH && shape->type != SHAPE_TERRAIN && shape->type != SHAPE_COMPOUND) {
    JPH_ConvexShape_SetDensity((JPH_ConvexShape*) shape->handle, density);
  }
}

float lovrShapeGetMass(Shape* shape) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    return 0.f;
  }

  JPH_MassProperties properties;
  JPH_Shape_GetMassProperties(shape->handle, &properties);
  return properties.mass;
}

void lovrShapeGetInertia(Shape* shape, float diagonal[3], float rotation[4]) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    vec3_set(diagonal, 0.f, 0.f, 0.f);
    quat_identity(rotation);
    return;
  }

  // eigenvalue decomposition ^^
}

void lovrShapeGetCenterOfMass(Shape* shape, float center[3]) {
  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);
  vec3_fromJolt(center, &centerOfMass);
}

void lovrShapeGetAABB(Shape* shape, float position[3], float orientation[4], float aabb[6]) {
  JPH_AABox box;
  if (!position && !orientation) {
    JPH_Shape_GetLocalBounds(shape->handle, &box);
  } else {
    JPH_RMatrix4x4 transform;
    JPH_Vec3 scale = { 1.f, 1.f, 1.f };
    mat4_fromPose(&transform.m11, position, orientation);
    JPH_Shape_GetWorldSpaceBounds(shape->handle, &transform, &scale, &box);
  }
  aabb[0] = box.min.x;
  aabb[1] = box.max.x;
  aabb[2] = box.min.y;
  aabb[3] = box.max.y;
  aabb[4] = box.min.z;
  aabb[5] = box.max.z;
}

BoxShape* lovrBoxShapeCreate(float dimensions[3]) {
  BoxShape* shape = lovrCalloc(sizeof(BoxShape));
  shape->ref = 1;
  shape->type = SHAPE_BOX;
  const JPH_Vec3 halfExtent = { dimensions[0] / 2.f, dimensions[1] / 2.f, dimensions[2] / 2.f };
  shape->handle = (JPH_Shape*) JPH_BoxShape_Create(&halfExtent, 0.f);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  return shape;
}

void lovrBoxShapeGetDimensions(BoxShape* shape, float dimensions[3]) {
  JPH_Vec3 halfExtent;
  JPH_BoxShape_GetHalfExtent((JPH_BoxShape*) shape->handle, &halfExtent);
  vec3_set(dimensions, halfExtent.x * 2.f, halfExtent.y * 2.f, halfExtent.z * 2.f);
}

SphereShape* lovrSphereShapeCreate(float radius) {
  lovrCheck(radius > 0.f, "SphereShape radius must be positive");
  SphereShape* shape = lovrCalloc(sizeof(SphereShape));
  shape->ref = 1;
  shape->type = SHAPE_SPHERE;
  shape->handle = (JPH_Shape*) JPH_SphereShape_Create(radius);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  return shape;
}

float lovrSphereShapeGetRadius(SphereShape* shape) {
  return JPH_SphereShape_GetRadius((JPH_SphereShape*) shape->handle);
}

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CapsuleShape dimensions must be positive");
  CapsuleShape* shape = lovrCalloc(sizeof(CapsuleShape));
  shape->ref = 1;
  shape->type = SHAPE_CAPSULE;
  shape->handle = (JPH_Shape*) JPH_CapsuleShape_Create(length / 2.f, radius);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  return shape;
}

float lovrCapsuleShapeGetRadius(CapsuleShape* shape) {
  return JPH_CapsuleShape_GetRadius((JPH_CapsuleShape*) shape->handle);
}

float lovrCapsuleShapeGetLength(CapsuleShape* shape) {
  return 2.f * JPH_CapsuleShape_GetHalfHeightOfCylinder((JPH_CapsuleShape*) shape->handle);
}

CylinderShape* lovrCylinderShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CylinderShape dimensions must be positive");
  CylinderShape* shape = lovrCalloc(sizeof(CylinderShape));
  shape->ref = 1;
  shape->type = SHAPE_CYLINDER;
  shape->handle = (JPH_Shape*) JPH_CylinderShape_Create(length / 2.f, radius);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  return shape;
}

float lovrCylinderShapeGetRadius(CylinderShape* shape) {
  return JPH_CylinderShape_GetRadius((JPH_CylinderShape*) shape->handle);
}

float lovrCylinderShapeGetLength(CylinderShape* shape) {
  return JPH_CylinderShape_GetHalfHeight((JPH_CylinderShape*) shape->handle) * 2.f;
}

ConvexShape* lovrConvexShapeCreate(float points[], uint32_t count) {
  ConvexShape* shape = lovrCalloc(sizeof(ConvexShape));
  shape->ref = 1;
  shape->type = SHAPE_CONVEX;
  JPH_ConvexHullShapeSettings* settings = JPH_ConvexHullShapeSettings_Create((const JPH_Vec3*) points, count, .05f);
  shape->handle = (JPH_Shape*) JPH_ConvexHullShapeSettings_CreateShape(settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
  return shape;
}

uint32_t lovrConvexShapeGetPointCount(ConvexShape* shape) {
  return JPH_ConvexHullShape_GetNumPoints((JPH_ConvexHullShape*) shape->handle);
}

void lovrConvexShapeGetPoint(ConvexShape* shape, uint32_t index, float point[3]) {
  lovrCheck(index < lovrConvexShapeGetPointCount(shape), "Invalid point index '%d'", index + 1);
  JPH_Vec3 v;
  JPH_ConvexHullShape_GetPoint((JPH_ConvexHullShape*) shape->handle, index, &v);
  vec3_fromJolt(point, &v);
}

uint32_t lovrConvexShapeGetFaceCount(ConvexShape* shape) {
  return JPH_ConvexHullShape_GetNumFaces((JPH_ConvexHullShape*) shape->handle);
}

uint32_t lovrConvexShapeGetFace(ConvexShape* shape, uint32_t index, uint32_t* pointIndices, uint32_t capacity) {
  lovrCheck(index < lovrConvexShapeGetFaceCount(shape), "Invalid face index '%d'", index + 1);
  return JPH_ConvexHullShape_GetFaceVertices((JPH_ConvexHullShape*) shape->handle, index, capacity, pointIndices);
}

MeshShape* lovrMeshShapeCreate(int vertexCount, float vertices[], int indexCount, uint32_t indices[]) {
  MeshShape* shape = lovrCalloc(sizeof(MeshShape));
  shape->ref = 1;
  shape->type = SHAPE_MESH;

  int triangleCount = indexCount / 3;
  JPH_IndexedTriangle* indexedTriangles = lovrMalloc(triangleCount * sizeof(JPH_IndexedTriangle));
  for (int i = 0; i < triangleCount; i++) {
    indexedTriangles[i].i1 = indices[i * 3 + 0];
    indexedTriangles[i].i2 = indices[i * 3 + 1];
    indexedTriangles[i].i3 = indices[i * 3 + 2];
    indexedTriangles[i].materialIndex = 0;
  }
  JPH_MeshShapeSettings* shape_settings = JPH_MeshShapeSettings_Create2(
    (const JPH_Vec3*) vertices,
    vertexCount,
    indexedTriangles,
    triangleCount);
  shape->handle = (JPH_Shape*) JPH_MeshShapeSettings_CreateShape(shape_settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) shape_settings);
  lovrFree(indexedTriangles);
  // Note that we're responsible for freeing the vertices/indices when we're done with them because
  // ODE took ownership of mesh data.  If ODE gets removed, we should probably get rid of this and
  // have the caller free the vertices/indices themselves.
  lovrFree(vertices);
  lovrFree(indices);
  return shape;
}

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t n, float scaleXZ, float scaleY) {
  TerrainShape* shape = lovrCalloc(sizeof(TerrainShape));
  shape->ref = 1;
  shape->type = SHAPE_TERRAIN;
  const JPH_Vec3 offset = {
    .x = -.5f * scaleXZ,
    .y = 0.f,
    .z = -.5f * scaleXZ
  };
  const JPH_Vec3 scale = {
    .x = scaleXZ / (n - 1),
    .y = scaleY,
    .z = scaleXZ / (n - 1)
  };

  JPH_HeightFieldShapeSettings* shape_settings = JPH_HeightFieldShapeSettings_Create(vertices, &offset, &scale, n);
  shape->handle = (JPH_Shape*) JPH_HeightFieldShapeSettings_CreateShape(shape_settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) shape_settings);
  return shape;
}

CompoundShape* lovrCompoundShapeCreate(Shape** shapes, vec3 positions, quat orientations, uint32_t count, bool freeze) {
  lovrCheck(!freeze || count >= 2, "A frozen CompoundShape must contain at least two shapes");

  CompoundShape* shape = lovrCalloc(sizeof(CompoundShape));
  shape->ref = 1;
  shape->type = SHAPE_COMPOUND;

  JPH_CompoundShapeSettings* settings = freeze ?
    (JPH_CompoundShapeSettings*) JPH_StaticCompoundShapeSettings_Create() :
    (JPH_CompoundShapeSettings*) JPH_MutableCompoundShapeSettings_Create();

  for (uint32_t i = 0; i < count; i++) {
    lovrCheck(shapes[i]->type != SHAPE_COMPOUND, "Currently, nesting compound shapes is not supported");
    JPH_Vec3 position = { positions[3 * i + 0], positions[3 * i + 1], positions[3 * i + 2] };
    JPH_Quat rotation = { orientations[4 * i + 0], orientations[4 * i + 1], orientations[4 * i + 2], orientations[4 * i + 3] };
    JPH_CompoundShapeSettings_AddShape2(settings, &position, &rotation, shapes[i]->handle, 0);
    lovrRetain(shapes[i]);
  }

  if (freeze) {
    shape->handle = (JPH_Shape*) JPH_StaticCompoundShape_Create((JPH_StaticCompoundShapeSettings*) settings);
  } else {
    shape->handle = (JPH_Shape*) JPH_MutableCompoundShape_Create((JPH_MutableCompoundShapeSettings*) settings);
  }

  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
  return shape;
}

bool lovrCompoundShapeIsFrozen(CompoundShape* shape) {
  return JPH_Shape_GetSubType(shape->handle) == JPH_ShapeSubType_StaticCompound;
}

void lovrCompoundShapeAddChild(CompoundShape* shape, Shape* child, float* position, float* orientation) {
  lovrCheck(!lovrCompoundShapeIsFrozen(shape), "CompoundShape is frozen and can not be changed");
  lovrCheck(child->type != SHAPE_COMPOUND, "Currently, nesting compound shapes is not supported");
  JPH_Vec3 pos = { position[0], position[1], position[2] };
  JPH_Quat rot = { orientation[0], orientation[1], orientation[2], orientation[3] };
  JPH_MutableCompoundShape_AddShape((JPH_MutableCompoundShape*) shape->handle, &pos, &rot, child->handle, 0);
  lovrRetain(child);
}

void lovrCompoundShapeReplaceChild(CompoundShape* shape, uint32_t index, Shape* child, float* position, float* orientation) {
  lovrCheck(!lovrCompoundShapeIsFrozen(shape), "CompoundShape is frozen and can not be changed");
  lovrCheck(child->type != SHAPE_COMPOUND, "Currently, nesting compound shapes is not supported");
  lovrCheck(index < lovrCompoundShapeGetChildCount(shape), "CompoundShape has no child at index %d", index + 1);
  JPH_Vec3 pos = { position[0], position[1], position[2] };
  JPH_Quat rot = { orientation[0], orientation[1], orientation[2], orientation[3] };
  lovrRelease(lovrCompoundShapeGetChild(shape, index), lovrShapeDestroy);
  JPH_MutableCompoundShape_ModifyShape2((JPH_MutableCompoundShape*) shape->handle, index, &pos, &rot, child->handle);
  lovrRetain(child);
}

void lovrCompoundShapeRemoveChild(CompoundShape* shape, uint32_t index) {
  lovrCheck(!lovrCompoundShapeIsFrozen(shape), "CompoundShape is frozen and can not be changed");
  lovrCheck(index < lovrCompoundShapeGetChildCount(shape), "CompoundShape has no child at index %d", index + 1);
  Shape* child = lovrCompoundShapeGetChild(shape, index);
  JPH_MutableCompoundShape_RemoveShape((JPH_MutableCompoundShape*) shape->handle, index);
  lovrRelease(child, lovrShapeDestroy);
}

Shape* lovrCompoundShapeGetChild(CompoundShape* shape, uint32_t index) {
  if (index < lovrCompoundShapeGetChildCount(shape)) {
    const JPH_Shape* child;
    JPH_CompoundShape_GetSubShape((JPH_CompoundShape*) shape->handle, index, &child, NULL, NULL, NULL);
    return (Shape*) (uintptr_t) JPH_Shape_GetUserData(child);
  } else {
    return NULL;
  }
}

uint32_t lovrCompoundShapeGetChildCount(CompoundShape* shape) {
  return JPH_CompoundShape_GetNumSubShapes((JPH_CompoundShape*) shape->handle);
}

void lovrCompoundShapeGetChildOffset(CompoundShape* shape, uint32_t index, float position[3], float orientation[4]) {
  lovrCheck(index < lovrCompoundShapeGetChildCount(shape), "CompoundShape has no child at index %d", index + 1);
  const JPH_Shape* child;
  JPH_Vec3 p;
  JPH_Quat q;
  uint32_t userData;
  JPH_CompoundShape_GetSubShape((JPH_CompoundShape*) shape->handle, index, &child, &p, &q, &userData);
  vec3_fromJolt(position, &p);
  quat_fromJolt(orientation, &q);
}

void lovrCompoundShapeSetChildOffset(CompoundShape* shape, uint32_t index, float position[3], float orientation[4]) {
  lovrCheck(!lovrCompoundShapeIsFrozen(shape), "CompoundShape is frozen and can not be changed");
  lovrCheck(index < lovrCompoundShapeGetChildCount(shape), "CompoundShape has no child at index %d", index + 1);
  JPH_MutableCompoundShape_ModifyShape((JPH_MutableCompoundShape*) shape->handle, index, vec3_toJolt(position), quat_toJolt(orientation));
}

// Joints

static void lovrJointGetAnchors(Joint* joint, float anchor1[3], float anchor2[3]) {
  JPH_Body* body1 = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_Body* body2 = JPH_TwoBodyConstraint_GetBody2((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_RMatrix4x4 centerOfMassTransform1;
  JPH_RMatrix4x4 centerOfMassTransform2;
  JPH_Body_GetCenterOfMassTransform(body1, &centerOfMassTransform1);
  JPH_Body_GetCenterOfMassTransform(body2, &centerOfMassTransform2);
  JPH_Matrix4x4 constraintToBody1;
  JPH_Matrix4x4 constraintToBody2;
  JPH_TwoBodyConstraint_GetConstraintToBody1Matrix((JPH_TwoBodyConstraint*) joint->constraint, &constraintToBody1);
  JPH_TwoBodyConstraint_GetConstraintToBody2Matrix((JPH_TwoBodyConstraint*) joint->constraint, &constraintToBody2);
  mat4_mulVec4(&centerOfMassTransform1.m11, &constraintToBody1.m41);
  mat4_mulVec4(&centerOfMassTransform2.m11, &constraintToBody2.m41);
  anchor1[0] = constraintToBody1.m41;
  anchor1[1] = constraintToBody1.m42;
  anchor1[2] = constraintToBody1.m43;
  anchor2[0] = constraintToBody2.m41;
  anchor2[1] = constraintToBody2.m42;
  anchor2[2] = constraintToBody2.m43;
}

static JointNode* lovrJointGetNode(Joint* joint, Collider* collider) {
  return collider == lovrJointGetColliderA(joint) ? &joint->a : &joint->b;
}

void lovrJointInit(Joint* joint, Collider* a, Collider* b) {
  World* world = a->world;

  if (a) {
    if (a->joints) {
      joint->a.next = a->joints;
      lovrJointGetNode(a->joints, a)->prev = joint;
    }

    a->joints = joint;
  }

  if (b) {
    if (b->joints) {
      joint->b.next = b->joints;
      lovrJointGetNode(b->joints, b)->prev = joint;
    }

    b->joints = joint;
  }

  if (world->joints) {
    joint->world.next = world->joints;
    world->joints->world.prev = joint;
  }

  world->joints = joint;
  world->jointCount++;
}

void lovrJointDestroy(void* ref) {
  Joint* joint = ref;
  lovrJointDestroyData(joint);
  lovrFree(joint);
}

void lovrJointDestroyData(Joint* joint) {
  if (!joint->constraint) {
    return;
  }

  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
  World* world = a ? a->world : b->world;
  JointNode* node;

  if (a) {
    node = &joint->a;
    if (node->next) lovrJointGetNode(node->next, a)->prev = node->prev;
    if (node->prev) lovrJointGetNode(node->prev, a)->next = node->next;
    else a->joints = node->next;
  }

  if (b) {
    node = &joint->b;
    if (node->next) lovrJointGetNode(node->next, b)->prev = node->prev;
    if (node->prev) lovrJointGetNode(node->prev, b)->next = node->next;
    else b->joints = node->next;
  }

  node = &joint->world;
  if (node->next) node->next->world.prev = node->prev;
  if (node->prev) node->prev->world.next = node->next;
  else world->joints = joint->world.next;

  JPH_PhysicsSystem_RemoveConstraint(world->system, joint->constraint);
  JPH_Constraint_Destroy(joint->constraint);
  joint->constraint = NULL;
  world->jointCount--;

  lovrRelease(joint, lovrJointDestroy);
}

bool lovrJointIsDestroyed(Joint* joint) {
  return !joint->constraint;
}

JointType lovrJointGetType(Joint* joint) {
  return joint->type;
}

Collider* lovrJointGetColliderA(Joint* joint) {
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  return (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
}

Collider* lovrJointGetColliderB(Joint* joint) {
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  return (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
}

Joint* lovrJointGetNext(Joint* joint, Collider* collider) {
  return lovrJointGetNode(joint, collider)->next;
}

uint32_t lovrJointGetPriority(Joint* joint) {
  return JPH_Constraint_GetConstraintPriority(joint->constraint);
}

void lovrJointSetPriority(Joint* joint, uint32_t priority) {
  JPH_Constraint_SetConstraintPriority(joint->constraint, priority);
}

bool lovrJointIsEnabled(Joint* joint) {
  return JPH_Constraint_GetEnabled(joint->constraint);
}

void lovrJointSetEnabled(Joint* joint, bool enable) {
  JPH_Constraint_SetEnabled(joint->constraint, enable);
}

float lovrJointGetForce(Joint* joint) {
  JPH_Vec3 v;
  float force[3], x, y;
  switch (joint->type) {
    case JOINT_WELD:
      JPH_FixedConstraint_GetTotalLambdaPosition((JPH_FixedConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v));
    case JOINT_BALL:
      JPH_PointConstraint_GetTotalLambdaPosition((JPH_PointConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v));
    case JOINT_DISTANCE:
      return JPH_DistanceConstraint_GetTotalLambdaPosition((JPH_DistanceConstraint*) joint->constraint);
    case JOINT_HINGE:
      JPH_HingeConstraint_GetTotalLambdaPosition((JPH_HingeConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v));
    case JOINT_SLIDER:
      JPH_SliderConstraint_GetTotalLambdaPosition((JPH_SliderConstraint*) joint->constraint, &x, &y);
      return sqrtf((x * x) + (y * y));
    default: return 0.f;
  }
}

float lovrJointGetTorque(Joint* joint) {
  JPH_Vec3 v;
  float torque[3], x, y;
  switch (joint->type) {
    case JOINT_WELD:
      JPH_FixedConstraint_GetTotalLambdaRotation((JPH_FixedConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(torque, &v));
    case JOINT_BALL:
      return 0.f;
    case JOINT_DISTANCE:
      return 0.f;
    case JOINT_HINGE:
      JPH_HingeConstraint_GetTotalLambdaRotation((JPH_HingeConstraint*) joint->constraint, &x, &y);
      return sqrtf((x * x) + (y * y));
    case JOINT_SLIDER:
      JPH_SliderConstraint_GetTotalLambdaRotation((JPH_SliderConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(torque, &v));
    default: return 0.f;
  }
}

// WeldJoint

WeldJoint* lovrWeldJointCreate(Collider* a, Collider* b, float anchor[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  WeldJoint* joint = lovrCalloc(sizeof(WeldJoint));
  joint->ref = 1;
  joint->type = JOINT_WELD;

  JPH_FixedConstraintSettings* settings = JPH_FixedConstraintSettings_Create();
  JPH_FixedConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_FixedConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  joint->constraint = (JPH_Constraint*) JPH_FixedConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrWeldJointGetAnchors(WeldJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

// BallJoint

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  BallJoint* joint = lovrCalloc(sizeof(BallJoint));
  joint->ref = 1;
  joint->type = JOINT_BALL;

  JPH_PointConstraintSettings* settings = JPH_PointConstraintSettings_Create();
  JPH_PointConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_PointConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  joint->constraint = (JPH_Constraint*) JPH_PointConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrBallJointGetAnchors(BallJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

// ConeJoint

ConeJoint* lovrConeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  ConeJoint* joint = lovrCalloc(sizeof(ConeJoint));
  joint->ref = 1;
  joint->type = JOINT_CONE;

  JPH_ConeConstraintSettings* settings = JPH_ConeConstraintSettings_Create();
  JPH_ConeConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_ConeConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  JPH_ConeConstraintSettings_SetTwistAxis1(settings, vec3_toJolt(axis));
  JPH_ConeConstraintSettings_SetTwistAxis2(settings, vec3_toJolt(axis));
  joint->constraint = (JPH_Constraint*) JPH_ConeConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_PhysicsSystem_AddConstraint((a ? a : b)->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrConeJointGetAnchors(ConeJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

void lovrConeJointGetAxis(ConeJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_ConeConstraintSettings* settings = (JPH_ConeConstraintSettings*) JPH_Constraint_GetConstraintSettings((JPH_Constraint*) joint->constraint);
  JPH_ConeConstraintSettings_GetTwistAxis1(settings, &resultAxis);
  JPH_Body* body1 = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_RMatrix4x4 centerOfMassTransform;
  JPH_Body_GetCenterOfMassTransform(body1, &centerOfMassTransform);
  JPH_Matrix4x4 constraintToBody;
  JPH_TwoBodyConstraint_GetConstraintToBody1Matrix((JPH_TwoBodyConstraint*) joint->constraint, &constraintToBody);
  float translation[4] = {
    resultAxis.x,
    resultAxis.y,
    resultAxis.z,
    0.f
  };
  mat4_mulVec4(&centerOfMassTransform.m11, translation);
  vec3_init(axis, translation);
}

float lovrConeJointGetLimit(ConeJoint* joint) {
  return acosf(JPH_ConeConstraint_GetCosHalfConeAngle((JPH_ConeConstraint*) joint->constraint));
}

void lovrConeJointSetLimit(ConeJoint* joint, float angle) {
  JPH_ConeConstraint_SetHalfConeAngle((JPH_ConeConstraint*) joint->constraint, CLAMP(angle, 0.f, (float) M_PI));
}

// DistanceJoint

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  DistanceJoint* joint = lovrCalloc(sizeof(DistanceJoint));
  joint->ref = 1;
  joint->type = JOINT_DISTANCE;

  JPH_DistanceConstraintSettings* settings = JPH_DistanceConstraintSettings_Create();
  JPH_DistanceConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor1));
  JPH_DistanceConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor2));
  joint->constraint = (JPH_Constraint*) JPH_DistanceConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrDistanceJointGetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

void lovrDistanceJointGetLimits(DistanceJoint* joint, float* min, float* max) {
  *min = JPH_DistanceConstraint_GetMinDistance((JPH_DistanceConstraint*) joint->constraint);
  *max = JPH_DistanceConstraint_GetMaxDistance((JPH_DistanceConstraint*) joint->constraint);
}

void lovrDistanceJointSetLimits(DistanceJoint* joint, float min, float max) {
  JPH_DistanceConstraint_SetDistance((JPH_DistanceConstraint*) joint->constraint, min, max);
}

void lovrDistanceJointGetSpring(DistanceJoint* joint, float* frequency, float* damping) {
  JPH_SpringSettings settings;
  JPH_DistanceConstraint_GetLimitsSpringSettings((JPH_DistanceConstraint*) joint->constraint, &settings);
  *frequency = settings.frequencyOrStiffness;
  *damping = settings.damping;
}

void lovrDistanceJointSetSpring(DistanceJoint* joint, float frequency, float damping) {
  JPH_DistanceConstraint_SetLimitsSpringSettings((JPH_DistanceConstraint*) joint->constraint, &(JPH_SpringSettings) {
    .frequencyOrStiffness = frequency,
    .damping = damping
  });
}

// HingeJoint

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  HingeJoint* joint = lovrCalloc(sizeof(HingeJoint));
  joint->ref = 1;
  joint->type = JOINT_HINGE;

  JPH_HingeConstraintSettings* settings = JPH_HingeConstraintSettings_Create();
  JPH_HingeConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_HingeConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  JPH_HingeConstraintSettings_SetHingeAxis1(settings, vec3_toJolt(axis));
  JPH_HingeConstraintSettings_SetHingeAxis2(settings, vec3_toJolt(axis));
  joint->constraint = (JPH_Constraint*) JPH_HingeConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrHingeJointGetAnchors(HingeJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors(joint, anchor1, anchor2);
}

void lovrHingeJointGetAxis(HingeJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_HingeConstraintSettings* settings = (JPH_HingeConstraintSettings*) JPH_Constraint_GetConstraintSettings((JPH_Constraint*) joint->constraint);
  JPH_HingeConstraintSettings_GetHingeAxis1(settings, &resultAxis);
  JPH_Body* body1 = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_RMatrix4x4 centerOfMassTransform;
  JPH_Body_GetCenterOfMassTransform(body1, &centerOfMassTransform);
  JPH_Matrix4x4 constraintToBody;
  JPH_TwoBodyConstraint_GetConstraintToBody1Matrix((JPH_TwoBodyConstraint*) joint->constraint, &constraintToBody);
  float translation[4] = {
    resultAxis.x,
    resultAxis.y,
    resultAxis.z,
    0.f
  };
  mat4_mulVec4(&centerOfMassTransform.m11, translation);
  vec3_init(axis, translation);
}

float lovrHingeJointGetAngle(HingeJoint* joint) {
  return -JPH_HingeConstraint_GetCurrentAngle((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointGetLimits(HingeJoint* joint, float* min, float* max) {
  *min = JPH_HingeConstraint_GetLimitsMin((JPH_HingeConstraint*) joint->constraint);
  *max = JPH_HingeConstraint_GetLimitsMax((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointSetLimits(HingeJoint* joint, float min, float max) {
  JPH_HingeConstraint_SetLimits((JPH_HingeConstraint*) joint->constraint, min, max);
}

float lovrHingeJointGetFriction(HingeJoint* joint) {
  return JPH_HingeConstraint_GetMaxFrictionTorque((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointSetFriction(HingeJoint* joint, float friction) {
  JPH_HingeConstraint_SetMaxFrictionTorque((JPH_HingeConstraint*) joint->constraint, friction);
}

void lovrHingeJointGetMotorTarget(HingeJoint* joint, TargetType* type, float* value) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  JPH_MotorState motorState = JPH_HingeConstraint_GetMotorState(constraint);
  *type = (TargetType) motorState;
  switch (motorState) {
    case JPH_MotorState_Velocity: *value = JPH_HingeConstraint_GetTargetAngularVelocity(constraint); break;
    case JPH_MotorState_Position: *value = JPH_HingeConstraint_GetTargetAngle(constraint); break;
    default: *value = 0.f; break;
  }
}

void lovrHingeJointSetMotorTarget(HingeJoint* joint, TargetType type, float value) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  switch (type) {
    case TARGET_VELOCITY:
      JPH_HingeConstraint_SetMotorState(constraint, JPH_MotorState_Velocity);
      JPH_HingeConstraint_SetTargetAngularVelocity(constraint, value);
      break;
    case TARGET_POSITION:
      JPH_HingeConstraint_SetMotorState(constraint, JPH_MotorState_Position);
      JPH_HingeConstraint_SetTargetAngle(constraint, value);
      break;
    default:
      JPH_HingeConstraint_SetMotorState(constraint, JPH_MotorState_Off);
      break;
  }
}

void lovrHingeJointGetMotorSpring(HingeJoint* joint, float* frequency, float* damping) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  *frequency = settings.springSettings.frequencyOrStiffness;
  *damping = settings.springSettings.damping;
}

void lovrHingeJointSetMotorSpring(HingeJoint* joint, float frequency, float damping) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  settings.springSettings.frequencyOrStiffness = frequency;
  settings.springSettings.damping = damping;
  JPH_HingeConstraint_SetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
}

void lovrHingeJointGetMaxMotorForce(HingeJoint* joint, float* positive, float* negative) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  *positive = settings.maxTorqueLimit;
  *negative = -settings.minTorqueLimit;
}

void lovrHingeJointSetMaxMotorForce(HingeJoint* joint, float positive, float negative) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  settings.minTorqueLimit = -negative;
  settings.maxTorqueLimit = positive;
  JPH_HingeConstraint_SetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
}

float lovrHingeJointGetMotorForce(HingeJoint* joint) {
  return JPH_HingeConstraint_GetTotalLambdaMotor((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointGetSpring(HingeJoint* joint, float* frequency, float* damping) {
  JPH_SpringSettings settings;
  JPH_HingeConstraint_GetLimitsSpringSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  *frequency = settings.frequencyOrStiffness;
  *damping = settings.damping;
}

void lovrHingeJointSetSpring(HingeJoint* joint, float frequency, float damping) {
  JPH_HingeConstraint_SetLimitsSpringSettings((JPH_HingeConstraint*) joint->constraint, &(JPH_SpringSettings) {
    .frequencyOrStiffness = frequency,
    .damping = damping
  });
}

// SliderJoint

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float axis[3]) {
  lovrCheck(a || b, "Joint requires at least one Collider");
  lovrCheck(!a || !b || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* bodyA = a ? a->body : NULL;
  JPH_Body* bodyB = b ? b->body : NULL;
  World* world = (a ? a : b)->world;

  SliderJoint* joint = lovrCalloc(sizeof(SliderJoint));
  joint->ref = 1;
  joint->type = JOINT_SLIDER;

  JPH_SliderConstraintSettings* settings = JPH_SliderConstraintSettings_Create();
  JPH_SliderConstraintSettings_SetSliderAxis(settings, vec3_toJolt(axis));
  joint->constraint = (JPH_Constraint*) JPH_SliderConstraintSettings_CreateConstraint(settings, bodyA, bodyB);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrSliderJointGetAnchors(SliderJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors(joint, anchor1, anchor2);
}

void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_SliderConstraintSettings* settings = (JPH_SliderConstraintSettings*) JPH_Constraint_GetConstraintSettings((JPH_Constraint*) joint->constraint);
  JPH_SliderConstraintSettings_GetSliderAxis(settings, &resultAxis);
  JPH_Body* body1 = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_RMatrix4x4 centerOfMassTransform;
  JPH_Body_GetCenterOfMassTransform(body1, &centerOfMassTransform);
  JPH_Matrix4x4 constraintToBody;
  JPH_TwoBodyConstraint_GetConstraintToBody1Matrix((JPH_TwoBodyConstraint*) joint->constraint, &constraintToBody);
  float translation[4] = {
    resultAxis.x,
    resultAxis.y,
    resultAxis.z,
    0.f
  };
  mat4_mulVec4(&centerOfMassTransform.m11, translation);
  axis[0] = translation[0];
  axis[1] = translation[1];
  axis[2] = translation[2];
}

float lovrSliderJointGetPosition(SliderJoint* joint) {
  return JPH_SliderConstraint_GetCurrentPosition((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointGetLimits(SliderJoint* joint, float* min, float* max) {
  *min = JPH_SliderConstraint_GetLimitsMin((JPH_SliderConstraint*) joint->constraint);
  *max = JPH_SliderConstraint_GetLimitsMax((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointSetLimits(SliderJoint* joint, float min, float max) {
  JPH_SliderConstraint_SetLimits((JPH_SliderConstraint*) joint->constraint, min, max);
}

float lovrSliderJointGetFriction(SliderJoint* joint) {
  return JPH_SliderConstraint_GetMaxFrictionForce((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointSetFriction(SliderJoint* joint, float friction) {
  JPH_SliderConstraint_SetMaxFrictionForce((JPH_SliderConstraint*) joint->constraint, friction);
}

void lovrSliderJointGetMotorTarget(SliderJoint* joint, TargetType* type, float* value) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  JPH_MotorState motorState = JPH_SliderConstraint_GetMotorState(constraint);
  *type = (TargetType) motorState;
  switch (motorState) {
    case JPH_MotorState_Velocity: *value = JPH_SliderConstraint_GetTargetVelocity(constraint); break;
    case JPH_MotorState_Position: *value = JPH_SliderConstraint_GetTargetPosition(constraint); break;
    default: *value = 0.f; break;
  }
}

void lovrSliderJointSetMotorTarget(SliderJoint* joint, TargetType type, float value) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  switch (type) {
    case TARGET_VELOCITY:
      JPH_SliderConstraint_SetMotorState(constraint, JPH_MotorState_Velocity);
      JPH_SliderConstraint_SetTargetVelocity(constraint, value);
      break;
    case TARGET_POSITION:
      JPH_SliderConstraint_SetMotorState(constraint, JPH_MotorState_Position);
      JPH_SliderConstraint_SetTargetPosition(constraint, value);
      break;
    default:
      JPH_SliderConstraint_SetMotorState(constraint, JPH_MotorState_Off);
      break;
  }
}

void lovrSliderJointGetMotorSpring(SliderJoint* joint, float* frequency, float* damping) {
  JPH_MotorSettings settings;
  JPH_SliderConstraint_GetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
  *frequency = settings.springSettings.frequencyOrStiffness;
  *damping = settings.springSettings.damping;
}

void lovrSliderJointSetMotorSpring(SliderJoint* joint, float frequency, float damping) {
  JPH_MotorSettings settings;
  JPH_SliderConstraint_GetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
  settings.springSettings.frequencyOrStiffness = frequency;
  settings.springSettings.damping = damping;
  JPH_SliderConstraint_SetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
}

void lovrSliderJointGetMaxMotorForce(SliderJoint* joint, float* positive, float* negative) {
  JPH_MotorSettings settings;
  JPH_SliderConstraint_GetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
  *positive = settings.maxForceLimit;
  *negative = -settings.minForceLimit;
}

void lovrSliderJointSetMaxMotorForce(SliderJoint* joint, float positive, float negative) {
  JPH_MotorSettings settings;
  JPH_SliderConstraint_GetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
  settings.minForceLimit = -negative;
  settings.maxForceLimit = positive;
  JPH_SliderConstraint_SetMotorSettings((JPH_SliderConstraint*) joint->constraint, &settings);
}

float lovrSliderJointGetMotorForce(SliderJoint* joint) {
  return JPH_SliderConstraint_GetTotalLambdaMotor((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointGetSpring(SliderJoint* joint, float* frequency, float* damping) {
  JPH_SpringSettings settings;
  JPH_SliderConstraint_GetLimitsSpringSettings((JPH_SliderConstraint*) joint->constraint, &settings);
  *frequency = settings.frequencyOrStiffness;
  *damping = settings.damping;
}

void lovrSliderJointSetSpring(SliderJoint* joint, float frequency, float damping) {
  JPH_SliderConstraint_SetLimitsSpringSettings((JPH_SliderConstraint*) joint->constraint, &(JPH_SpringSettings) {
    .frequencyOrStiffness = frequency,
    .damping = damping
  });
}
