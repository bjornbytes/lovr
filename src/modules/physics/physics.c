#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <threads.h>
#include <joltc.h>

struct Contact {
  uint32_t ref;
  Collider* colliderA;
  Collider* colliderB;
  const JPH_ContactManifold* manifold;
  JPH_ContactSettings* settings;
};

struct World {
  uint32_t ref;
  JPH_PhysicsSystem* system;
  JPH_BodyInterface* bodyInterfaceLocked;
  JPH_BodyInterface* bodyInterfaceNoLock;
  JPH_BodyActivationListener* activationListener;
  JPH_ObjectLayerPairFilter* objectLayerPairFilter;
  JPH_ContactListener* listener;
  Contact contact;
  Collider* colliders;
  Collider** activeColliders;
  uint32_t activeColliderCount;
  Joint* joints;
  uint32_t jointCount;
  WorldCallbacks callbacks;
  float defaultLinearDamping;
  float defaultAngularDamping;
  bool defaultIsSleepingAllowed;
  float inverseDelta;
  float interpolation;
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
  Joint* joints;
  Shape* shapes;
  uint8_t tag;
  bool enabled;
  bool automaticMass;
  uint32_t activeIndex;
  uintptr_t userdata;
  float lastPosition[4];
  float lastOrientation[4];
};

struct Shape {
  uint32_t ref;
  ShapeType type;
  JPH_Shape* handle;
  Collider* collider;
  uintptr_t userdata;
  Shape* next;
  uint32_t index;
  float translation[3];
  float rotation[4];
};

typedef struct {
  Joint* prev;
  Joint* next;
} JointNode;

struct Joint {
  uint32_t ref;
  JointType type;
  JPH_Constraint* constraint;
  uintptr_t userdata;
  JointNode a, b, world;
};

static thread_local struct {
  JPH_BroadPhaseLayerFilter* broadPhaseLayerFilter;
  JPH_ObjectLayerFilter* objectLayerFilter;
  uint32_t broadPhaseLayerMask;
  uint32_t objectLayerMask;
  bool locked;
} thread;

static struct {
  bool initialized;
  SphereShape* sphere;
  void (*freeUserData)(void* object, uintptr_t userdata);
} state;

#define vec3_toJolt(v) &(JPH_Vec3) { v[0], v[1], v[2] }
#define vec3_fromJolt(v, j) vec3_set(v, (j)->x, (j)->y, (j)->z)
#define quat_toJolt(q) &(JPH_Quat) { q[0], q[1], q[2], q[3] }
#define quat_fromJolt(q, j) quat_set(q, (j)->x, (j)->y, (j)->z, (j)->w)

enum { READ, WRITE };

static JPH_BodyInterface* getBodyInterface(Collider* collider, int access) {
  if (thread.locked) {
    lovrCheck(access == READ, "Tried to write to a Collider inside a collision callback");
    return collider->world->bodyInterfaceNoLock;
  } else {
    return collider->world->bodyInterfaceLocked;
  }
}

static uint8_t findTag(World* world, const char* name, size_t length) {
  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint8_t i = 0; i < world->tagCount; i++) {
    if (world->tagLookup[i] == hash) {
      return i;
    }
  }
  return 0xff;
}

static Shape* subshapeToShape(Collider* collider, JPH_SubShapeID id) {
  if (collider->shapes && collider->shapes->next) {
    const JPH_Shape* shape = JPH_BodyInterface_GetShape(getBodyInterface(collider, READ), collider->id);

    if (JPH_Shape_GetSubType(shape) == JPH_ShapeSubType_OffsetCenterOfMass) {
      shape = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape);
    }

    if (JPH_Shape_GetSubType(shape) != JPH_ShapeSubType_MutableCompound) {
      lovrUnreachable();
    }

    JPH_SubShapeID remainder;
    uint32_t index = JPH_CompoundShape_GetSubShapeIndexFromID((JPH_CompoundShape*) shape, id, &remainder);

    const JPH_Shape* child;
    JPH_CompoundShape_GetSubShape((JPH_CompoundShape*) shape, index, &child, NULL, NULL, NULL);
    return (Shape*) (uintptr_t) JPH_Shape_GetUserData(child);
  } else {
    return collider->shapes;
  }
}

static bool broadPhaseLayerFilter(void* userdata, JPH_BroadPhaseLayer layer) {
  return (thread.broadPhaseLayerMask & (1 << layer)) != 0;
}

static JPH_BroadPhaseLayerFilter* getBroadPhaseLayerFilter(World* world, uint32_t filter) {
  if (!thread.broadPhaseLayerFilter) {
    thread.broadPhaseLayerFilter = JPH_BroadPhaseLayerFilter_Create((JPH_BroadPhaseLayerFilter_Procs) {
      .ShouldCollide = broadPhaseLayerFilter
    }, NULL);
  }

  thread.broadPhaseLayerMask = 0;
  if (~world->staticTagMask & filter) thread.broadPhaseLayerMask |= 0x1;
  if ( world->staticTagMask & filter) thread.broadPhaseLayerMask |= 0x3;

  return thread.broadPhaseLayerFilter;
}

static bool objectLayerFilter(void* userdata, JPH_ObjectLayer layer) {
  return (thread.objectLayerMask & (1 << layer)) != 0;
}

static JPH_ObjectLayerFilter* getObjectLayerFilter(World* world, uint32_t filter) {
  if (!thread.objectLayerFilter) {
    thread.objectLayerFilter = JPH_ObjectLayerFilter_Create((JPH_ObjectLayerFilter_Procs) {
      .ShouldCollide = objectLayerFilter
    }, NULL);
  }

  // Never include objects on the last layer, reserved for colliders without shapes
  thread.objectLayerMask = filter & ~(1 << (world->tagCount + 1));
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

static JPH_ValidateResult onContactValidate(void* userdata, const JPH_Body* body1, const JPH_Body* body2, const JPH_RVec3* offset, const JPH_CollideShapeResult* result) {
  World* world = userdata;
  Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body1);
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body2);
  mtx_lock(&world->lock);
  thread.locked = true;
  bool accept = world->callbacks.filter(world->callbacks.userdata, world, a, b);
  thread.locked = false;
  mtx_unlock(&world->lock);
  return accept ?
    JPH_ValidateResult_AcceptAllContactsForThisBodyPair :
    JPH_ValidateResult_RejectAllContactsForThisBodyPair;
}

static void onContactPersisted(void* userdata, const JPH_Body* body1, const JPH_Body* body2, const JPH_ContactManifold* manifold, JPH_ContactSettings* settings) {
  World* world = userdata;
  JPH_BodyID id1 = JPH_Body_GetID(body1);
  JPH_BodyID id2 = JPH_Body_GetID(body2);
  if (world->callbacks.contact) {
    Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body1);
    Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body2);
    mtx_lock(&world->lock);
    thread.locked = true;
    world->contact.colliderA = a;
    world->contact.colliderB = b;
    world->contact.manifold = manifold;
    world->contact.settings = settings;
    world->callbacks.contact(world->callbacks.userdata, world, a, b, &world->contact);
    thread.locked = false;
    mtx_unlock(&world->lock);
  }
}

static void onContactAdded(void* userdata, const JPH_Body* body1, const JPH_Body* body2, const JPH_ContactManifold* manifold, JPH_ContactSettings* settings) {
  World* world = userdata;
  JPH_BodyID id1 = JPH_Body_GetID(body1);
  JPH_BodyID id2 = JPH_Body_GetID(body2);

  if (world->callbacks.enter && !JPH_PhysicsSystem_WereBodiesInContact(world->system, id1, id2)) {
    Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body1);
    Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData((JPH_Body*) body2);
    mtx_lock(&world->lock);
    thread.locked = true;
    world->contact.colliderA = a;
    world->contact.colliderB = b;
    world->contact.manifold = manifold;
    world->contact.settings = settings;
    world->callbacks.enter(world->callbacks.userdata, world, a, b, &world->contact);
    thread.locked = false;
    mtx_unlock(&world->lock);
  }

  onContactPersisted(userdata, body1, body2, manifold, settings);
}

static void onContactRemoved(void* userdata, const JPH_SubShapeIDPair* pair) {
  World* world = userdata;
  if (!JPH_PhysicsSystem_WereBodiesInContact(world->system, pair->Body1ID, pair->Body2ID)) {
    JPH_BodyInterface* interface = world->bodyInterfaceNoLock;
    Collider* a = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(interface, pair->Body1ID);
    Collider* b = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(interface, pair->Body2ID);
    if (a && b) {
      mtx_lock(&world->lock);
      thread.locked = true;
      world->callbacks.exit(world->callbacks.userdata, world, a, b);
      thread.locked = false;
      mtx_unlock(&world->lock);
    }
  }
}

bool lovrPhysicsInit(void (*freeUserData)(void* object, uintptr_t userdata)) {
  if (state.initialized) return true;
  JPH_Init();
  state.sphere = lovrSphereShapeCreate(.001f);
  state.freeUserData = freeUserData;
  return state.initialized = true;
}

void lovrPhysicsDestroy(void) {
  if (!state.initialized) return;
  lovrRelease(state.sphere, lovrSphereShapeDestroy);
  JPH_Shutdown();
  state.initialized = false;
}

World* lovrWorldCreate(WorldInfo* info) {
  World* world = lovrCalloc(sizeof(World));

  world->ref = 1;
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
  uint32_t objectLayerCount = info->tagCount + 2;
  JPH_BroadPhaseLayerInterface* broadPhaseLayerInterface = JPH_BroadPhaseLayerInterfaceTable_Create(objectLayerCount, broadPhaseLayerCount);
  world->objectLayerPairFilter = JPH_ObjectLayerPairFilterTable_Create(objectLayerCount);

  // Each tag gets its own object layer, also add a layer for untagged colliders and one for shapeless colliders
  for (uint32_t i = 0; i < objectLayerCount; i++) {
    bool isStatic = world->staticTagMask & (1 << i);
    JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(broadPhaseLayerInterface, i, isStatic);

    // Static tags never collide with other static tags, last layer never colliders with anything
    for (uint32_t j = i; j < objectLayerCount; j++) {
      if ((isStatic && world->staticTagMask & (1 << j)) || j == objectLayerCount - 1) {
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
  settings.allowSleeping = info->allowSleep;
  settings.baumgarte = CLAMP(info->stabilization, 0.f, 1.f);
  settings.penetrationSlop = MAX(info->maxPenetration, 0.f);
  settings.minVelocityForRestitution = MAX(info->restitutionThreshold, 0.f);
  settings.numVelocitySteps = MAX(settings.numVelocitySteps, 2);
  settings.numPositionSteps = MAX(settings.numPositionSteps, 1);
  JPH_PhysicsSystem_SetPhysicsSettings(world->system, &settings);

  world->bodyInterfaceNoLock = JPH_PhysicsSystem_GetBodyInterfaceNoLock(world->system);
  world->bodyInterfaceLocked = info->threadSafe ?
    JPH_PhysicsSystem_GetBodyInterface(world->system) :
    world->bodyInterfaceNoLock;

  world->activeColliders = lovrMalloc(info->maxColliders * sizeof(Collider*));
  world->activationListener = JPH_BodyActivationListener_Create((JPH_BodyActivationListener_Procs) {
    .OnBodyActivated = onAwake,
    .OnBodyDeactivated = onSleep
  }, world);

  JPH_PhysicsSystem_SetBodyActivationListener(world->system, world->activationListener);

  return world;
}

void lovrWorldDestroy(void* ref) {
  World* world = ref;
  lovrWorldDestruct(world);
  lovrFree(world);
}

void lovrWorldDestruct(World* world) {
  if (!world->system) {
    return;
  }

  while (world->colliders) {
    Collider* collider = world->colliders;
    Collider* next = collider->next;
    lovrColliderDestruct(collider);
    world->colliders = next;
  }

  if (world->listener) JPH_ContactListener_Destroy(world->listener);
  JPH_BodyActivationListener_Destroy(world->activationListener);
  lovrFree(world->activeColliders);

  for (uint32_t i = 0; i < world->tagCount; i++) {
    lovrFree(world->tags[i]);
  }
  mtx_destroy(&world->lock);

  JPH_PhysicsSystem_Destroy(world->system);
  world->system = NULL;
}

bool lovrWorldIsDestroyed(World* world) {
  return !world->system;
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
    uint8_t tag = findTag(world, string + invert, span - invert);
    lovrCheck(tag != 0xff, "Unknown tag in filter '%s'", string);
    if (invert) ignore |= (1 << tag);
    else accept |= (1 << tag);
    span += !!space;
    string += span;
    length -= span;
  }

  // Always set the unused 32nd tag bit, so that zero can be used for error
  return (accept ? accept : ~ignore) | 0x80000000;
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
  for (uint32_t i = 0; i < world->activeColliderCount; i++) {
    Collider* collider = world->activeColliders[i];

    JPH_RVec3 position;
    JPH_Body_GetPosition(collider->body, &position);
    vec3_fromJolt(collider->lastPosition, &position);

    JPH_Quat orientation;
    JPH_Body_GetRotation(collider->body, &orientation);
    quat_fromJolt(collider->lastOrientation, &orientation);
  }

  JPH_PhysicsSystem_Step(world->system, dt, 1);
  world->inverseDelta = 1.f / dt;
  world->interpolation = 0.f;
}

void lovrWorldInterpolate(World* world, float alpha) {
  world->interpolation = 1.f - alpha;
}

typedef struct {
  World* world;
  float* start;
  float* direction;
  CastCallback* callback;
  void* userdata;
} CastContext;

static float raycastCallback(void* arg, const JPH_RayCastResult* result) {
  CastResult hit;
  CastContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodyInterfaceNoLock, result->bodyID);
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

  return JPH_NarrowPhaseQuery_CastRay2(query, origin, dir, NULL, raycastCallback, &context, layerFilter, tagFilter, NULL, NULL);
}

static float shapecastCallback(void* arg, const JPH_ShapeCastResult* result) {
  CastResult hit;
  CastContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodyInterfaceNoLock, result->bodyID2);
  hit.shape = subshapeToShape(hit.collider, result->subShapeID2);
  vec3_fromJolt(hit.position, &result->contactPointOn2);
  JPH_Vec3 normal;
  JPH_Body_GetWorldSpaceSurfaceNormal(hit.collider->body, result->subShapeID2, &result->contactPointOn2, &normal);
  vec3_fromJolt(hit.normal, &normal);
  hit.fraction = result->fraction;
  return ctx->callback(ctx->userdata, &hit);
}

bool lovrWorldShapecast(World* world, Shape* shape, float pose[7], float end[3], uint32_t filter, CastCallback callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);

  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);

  JPH_RMatrix4x4 transform;
  mat4_fromPose(&transform.m11, pose, pose + 3);
  mat4_translate(&transform.m11, centerOfMass.x, centerOfMass.y, centerOfMass.z);

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

  return JPH_NarrowPhaseQuery_CastShape(query, shape->handle, &transform, dir, NULL, &offset, shapecastCallback, &context, layerFilter, tagFilter, NULL, NULL);
}

typedef struct {
  World* world;
  OverlapCallback* callback;
  void* userdata;
} OverlapContext;

static float overlapCallback(void* arg, const JPH_CollideShapeResult* result) {
  OverlapResult hit;
  OverlapContext* ctx = arg;
  hit.collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodyInterfaceNoLock, result->bodyID2);
  hit.shape = subshapeToShape(hit.collider, result->subShapeID2);
  vec3_fromJolt(hit.position, &result->contactPointOn2);
  vec3_fromJolt(hit.normal, &result->penetrationAxis);
  vec3_scale(vec3_normalize(hit.normal), result->penetrationDepth);
  return ctx->callback(ctx->userdata, &hit);
}

bool lovrWorldOverlapShape(World* world, Shape* shape, float pose[7], uint32_t filter, OverlapCallback* callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);

  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);

  JPH_RMatrix4x4 transform;
  mat4_fromPose(&transform.m11, pose, pose + 3);
  mat4_translate(&transform.m11, centerOfMass.x, centerOfMass.y, centerOfMass.z);

  JPH_Vec3 scale = { 1.f, 1.f, 1.f };
  JPH_RVec3 offset = { 0.f, 0.f, 0.f };

  OverlapContext context = {
    .world = world,
    .callback = callback,
    .userdata = userdata
  };

  JPH_BroadPhaseLayerFilter* layerFilter = getBroadPhaseLayerFilter(world, filter);
  JPH_ObjectLayerFilter* tagFilter = getObjectLayerFilter(world, filter);

  return JPH_NarrowPhaseQuery_CollideShape(query, shape->handle, &scale, &transform, NULL, &offset, overlapCallback, &context, layerFilter, tagFilter, NULL, NULL);
}

typedef struct {
  World* world;
  QueryCallback* callback;
  void* userdata;
} QueryContext;

static void queryCallback(void* arg, JPH_BodyID id) {
  QueryContext* ctx = arg;
  Collider* collider = (Collider*) (uintptr_t) JPH_BodyInterface_GetUserData(ctx->world->bodyInterfaceNoLock, id);
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

bool lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint8_t i = findTag(world, tag1, strlen(tag1));
  uint8_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != 0xff, "Unknown tag '%s'", tag1);
  lovrCheck(j != 0xff, "Unknown tag '%s'", tag2);
  JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, i, j);
  return true;
}

bool lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint8_t i = findTag(world, tag1, strlen(tag1));
  uint8_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != 0xff, "Unknown tag '%s'", tag1);
  lovrCheck(j != 0xff, "Unknown tag '%s'", tag2);
  JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, i, j);
  return true;
}

bool lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2, bool* enabled) {
  if (!tag1 || !tag2) return true;
  uint8_t i = findTag(world, tag1, strlen(tag1));
  uint8_t j = findTag(world, tag2, strlen(tag2));
  lovrCheck(i != 0xff, "Unknown tag '%s'", tag1);
  lovrCheck(j != 0xff, "Unknown tag '%s'", tag2);
  *enabled = JPH_ObjectLayerPairFilterTable_ShouldCollide(world->objectLayerPairFilter, i, j);
  return true;
}

void lovrWorldSetCallbacks(World* world, WorldCallbacks* callbacks) {
  if (world->listener) {
    JPH_ContactListener_Destroy(world->listener);
    world->listener = NULL;
  }

  if (!callbacks || (!callbacks->filter && !callbacks->enter && !callbacks->exit && !callbacks->contact)) {
    JPH_PhysicsSystem_SetContactListener(world->system, NULL);
  } else {
    world->callbacks = *callbacks;
    world->listener = JPH_ContactListener_Create((JPH_ContactListener_Procs) {
      .OnContactValidate = callbacks->filter ? onContactValidate : NULL,
      .OnContactAdded = (callbacks->enter || callbacks->contact) ? onContactAdded : NULL,
      .OnContactPersisted = callbacks->contact ? onContactPersisted : NULL,
      .OnContactRemoved = callbacks->exit ? onContactRemoved : NULL
    }, world);

    JPH_PhysicsSystem_SetContactListener(world->system, world->listener);
  }
}

// Deprecated
int lovrWorldGetStepCount(World* world) { return 1; }
void lovrWorldSetStepCount(World* world, int iterations) {}
float lovrWorldGetResponseTime(World* world) { return 0.f; }
void lovrWorldSetResponseTime(World* world, float responseTime) {}
float lovrWorldGetTightness(World* world) { return 0.f; }
void lovrWorldSetTightness(World* world, float tightness) {}
bool lovrWorldIsSleepingAllowed(World* world) { return world->defaultIsSleepingAllowed; }
void lovrWorldSetSleepingAllowed(World* world, bool allowed) { world->defaultIsSleepingAllowed = allowed; }
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold) { *damping = world->defaultLinearDamping, *threshold = 0.f; }
void lovrWorldSetLinearDamping(World* world, float damping, float threshold) { world->defaultLinearDamping = damping; }
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold) { *damping = world->defaultAngularDamping, *threshold = 0.f; }
void lovrWorldSetAngularDamping(World* world, float damping, float threshold) { world->defaultAngularDamping = damping; }

// Collider

static void adjustJoints(Collider* collider, JPH_Vec3* oldCenter, JPH_Vec3* newCenter) {
  if (collider->joints) {
    JPH_Vec3 delta = { newCenter->x - oldCenter->x, newCenter->y - oldCenter->y, newCenter->z - oldCenter->z };
    for (Joint* joint = collider->joints; joint; joint = lovrJointGetNext(joint, collider)) {
      JPH_Constraint_NotifyShapeChanged(joint->constraint, collider->id, &delta);
    }
  }
}

Collider* lovrColliderCreate(World* world, float position[3], Shape* shape) {
  uint32_t count = JPH_PhysicsSystem_GetNumBodies(world->system);
  uint32_t limit = JPH_PhysicsSystem_GetMaxBodies(world->system);
  lovrCheck(count < limit, "Too many colliders!");
  lovrCheck(!shape || !shape->collider, "Shape is already attached to a collider!");

  Collider* collider = lovrCalloc(sizeof(Collider));
  collider->ref = 1;
  collider->world = world;
  collider->tag = 0xff;
  collider->enabled = true;
  collider->automaticMass = true;

  if (shape) {
    collider->shapes = shape;
    shape->collider = collider;
    shape->index = 0;
    lovrRetain(shape);
  } else {
    shape = state.sphere;
  }

  JPH_RVec3* p = vec3_toJolt(position);
  JPH_Quat q = { 0.f, 0.f, 0.f, 1.f };
  JPH_MotionType type = JPH_Shape_MustBeStatic(shape->handle) ? JPH_MotionType_Static : JPH_MotionType_Dynamic;
  JPH_ObjectLayer objectLayer = shape != state.sphere ? world->tagCount : world->tagCount + 1; // Untagged/shapeless layer
  JPH_BodyCreationSettings* settings = JPH_BodyCreationSettings_Create3(shape->handle, p, &q, type, objectLayer);
  collider->body = JPH_BodyInterface_CreateBody(world->bodyInterfaceLocked, settings);
  collider->id = JPH_Body_GetID(collider->body);
  JPH_Body_SetUserData(collider->body, (uint64_t) (uintptr_t) collider);
  JPH_BodyCreationSettings_Destroy(settings);

  JPH_BodyInterface_AddBody(world->bodyInterfaceLocked, collider->id, JPH_Activation_Activate);

  vec3_init(collider->lastPosition, position);
  quat_identity(collider->lastOrientation);

  if (type == JPH_MotionType_Dynamic) {
    lovrColliderSetLinearDamping(collider, world->defaultLinearDamping);
    lovrColliderSetAngularDamping(collider, world->defaultAngularDamping);
    lovrColliderSetSleepingAllowed(collider, world->defaultIsSleepingAllowed);
  }

  if (world->colliders) {
    collider->next = world->colliders;
    collider->next->prev = collider;
  }

  world->colliders = collider;

  lovrRetain(collider);
  return collider;
}

void lovrColliderDestroy(void* ref) {
  Collider* collider = ref;
  lovrColliderDestruct(collider);
  lovrFree(collider);
}

void lovrColliderDestruct(Collider* collider) {
  if (!collider->body) {
    return;
  }

  if (state.freeUserData) {
    state.freeUserData(collider, collider->userdata);
  }

  // Joints

  Joint* joint = collider->joints;

  while (joint) {
    Joint* next = lovrJointGetNext(joint, collider);
    lovrJointDestruct(joint);
    joint = next;
  }

  // Shapes

  Shape* shape = collider->shapes;

  while (shape) {
    Shape* next = shape->next;
    shape->collider = NULL;
    shape->next = NULL;
    shape->index = ~0u;
    lovrShapeDestruct(shape);
    lovrRelease(shape, lovrShapeDestroy);
    shape = next;
  }

  JPH_BodyInterface* interface = getBodyInterface(collider, READ);
  JPH_Shape* handle = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);
  JPH_ShapeSubType type = JPH_Shape_GetSubType(handle);

  if (type == JPH_ShapeSubType_OffsetCenterOfMass) {
    JPH_Shape* inner = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) handle);
    JPH_Shape_Destroy(handle);
    handle = inner;
    type = JPH_Shape_GetSubType(handle);
  }

  if (type == JPH_ShapeSubType_MutableCompound) {
    JPH_Shape_Destroy(handle);
  }

  // Body

  World* world = collider->world;
  bool added = JPH_BodyInterface_IsAdded(world->bodyInterfaceLocked, collider->id);
  if (added) JPH_BodyInterface_RemoveBody(world->bodyInterfaceLocked, collider->id);
  JPH_BodyInterface_DestroyBody(world->bodyInterfaceLocked, collider->id);
  collider->body = NULL;

  if (collider->next) collider->next->prev = collider->prev;
  if (collider->prev) collider->prev->next = collider->next;
  if (world->colliders == collider) world->colliders = collider->next;
  collider->next = collider->prev = NULL;
  lovrRelease(collider, lovrColliderDestroy);
}

bool lovrColliderIsDestroyed(Collider* collider) {
  return !collider->body;
}

uintptr_t lovrColliderGetUserData(Collider* collider) {
  return collider->userdata;
}

void lovrColliderSetUserData(Collider* collider, uintptr_t userdata) {
  if (state.freeUserData) state.freeUserData(collider, collider->userdata);
  collider->userdata = userdata;
}

bool lovrColliderIsEnabled(Collider* collider) {
  return collider->enabled;
}

bool lovrColliderSetEnabled(Collider* collider, bool enable) {
  if (collider->enabled == enable) return true;

  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  if (enable) {
    JPH_BodyInterface_AddBody(interface, collider->id, JPH_Activation_DontActivate);
  } else {
    JPH_BodyInterface_RemoveBody(interface, collider->id);
  }
  collider->enabled = enable;
  return true;
}

World* lovrColliderGetWorld(Collider* collider) {
  return collider->world;
}

Joint* lovrColliderGetJoints(Collider* collider, Joint* joint) {
  return joint ? lovrJointGetNext(joint, collider) : collider->joints;
}

Shape* lovrColliderGetShapes(Collider* collider, Shape* shape) {
  return shape ? shape->next : collider->shapes;
}

bool lovrColliderAddShape(Collider* collider, Shape* shape) {
  lovrCheck(!shape->collider, "Shape is already attached to a Collider");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  // Set the Collider to static if the new shape requires it
  if (JPH_Shape_MustBeStatic(shape->handle)) {
    JPH_BodyInterface_SetMotionType(interface, collider->id, JPH_MotionType_Static, JPH_Activation_DontActivate);
  }

  JPH_Shape* handle = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);
  JPH_Shape* offsetCenterOfMass = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_OffsetCenterOfMass ? handle : NULL;

  JPH_Vec3 oldCenter;
  JPH_Shape_GetCenterOfMass(handle, &oldCenter);

  if (offsetCenterOfMass) {
    handle = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) handle);
  }

  bool alreadyCompound = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_MutableCompound;

  JPH_Vec3* position = vec3_toJolt(shape->translation);
  JPH_Quat* rotation = quat_toJolt(shape->rotation);

  // Create or modify the MutableCompoundShape
  if (alreadyCompound) {
    JPH_MutableCompoundShape_AddShape((JPH_MutableCompoundShape*) handle, position, rotation, shape->handle, 0);
    shape->index = JPH_CompoundShape_GetNumSubShapes((JPH_CompoundShape*) handle) - 1;
  } else if (handle == state.sphere->handle) {
    // If the shape is at the origin, use it directly, otherwise wrap in a compound shape with an offset
    if (vec3_length(shape->translation) < 1e-6 && shape->rotation[3] > .999f) {
      handle = shape->handle;
      shape->index = 0;
    } else {
      JPH_MutableCompoundShapeSettings* settings = JPH_MutableCompoundShapeSettings_Create();
      JPH_CompoundShapeSettings_AddShape2((JPH_CompoundShapeSettings*) settings, position, rotation, shape->handle, 0);
      handle = (JPH_Shape*) JPH_MutableCompoundShape_Create(settings);
      JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
      shape->index = 0;
    }
  } else {
    float identity[] = { 0.f, 0.f, 0.f, 1.f };
    JPH_MutableCompoundShapeSettings* settings = JPH_MutableCompoundShapeSettings_Create();
    JPH_CompoundShapeSettings_AddShape2((JPH_CompoundShapeSettings*) settings, vec3_toJolt(identity), quat_toJolt(identity), handle, 0);
    JPH_CompoundShapeSettings_AddShape2((JPH_CompoundShapeSettings*) settings, position, rotation, shape->handle, 0);
    handle = (JPH_Shape*) JPH_MutableCompoundShape_Create(settings);
    JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
    shape->index = 1;
  }

  if (alreadyCompound && collider->automaticMass) {
    JPH_MutableCompoundShape_AdjustCenterOfMass((JPH_MutableCompoundShape*) handle);
  }

  JPH_Vec3 newCenter;
  JPH_Shape_GetCenterOfMass(handle, &newCenter);

  bool hasMass = !JPH_Shape_MustBeStatic(handle);

  // Adjust mass
  if (alreadyCompound) {
    if (collider->automaticMass) {
      if (offsetCenterOfMass) {
        // Set the shape to the CompoundShape, replacing the OffsetCenterOfMassShape.  This takes
        // care of recomputing the mass, center, etc.
        JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
        JPH_Shape_Destroy(offsetCenterOfMass);
      } else {
        // If the shape is already the CompoundShape, use NotifyShapeChanged to update mass/center
        JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
      }
    } else {
      // Mark the shape as changed so the AABB updates, but don't change the mass/center.
      JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, false, JPH_Activation_DontActivate);
    }
  } else {
    if (collider->automaticMass) {
      // Replace the existing shape with the new shape, adjusting all the mass properties
      JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
      adjustJoints(collider, &oldCenter, &newCenter);
    } else {
      // If automaticMass=false, use an OffsetCenterOfMassShape to keep the center of mass the same
      JPH_Vec3 offset = { oldCenter.x - newCenter.x, oldCenter.y - newCenter.y, oldCenter.z - newCenter.z };
      JPH_Shape* wrapper = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, handle);
      JPH_BodyInterface_SetShape(interface, collider->id, wrapper, false, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
    }
  }

  // Colliders with zero shapes have to go on a special layer, so if we add a shape to a collider
  // without any shapes, we gotta move it onto the right object layer
  if (!collider->shapes) {
    JPH_ObjectLayer objectLayer = collider->tag == 0xff ? collider->world->tagCount : collider->tag;
    JPH_BodyInterface_SetObjectLayer(interface, collider->id, objectLayer);
  }

  // Bookkeeping
  shape->collider = collider;
  shape->next = collider->shapes;
  collider->shapes = shape;
  lovrRetain(shape);
  return true;
}

bool lovrColliderRemoveShape(Collider* collider, Shape* shape) {
  lovrCheck(shape->collider == collider, "Shape is not attached to this Collider");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  JPH_Shape* handle = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);
  JPH_Shape* offsetCenterOfMass = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_OffsetCenterOfMass ? handle : NULL;

  JPH_Vec3 oldCenter;
  JPH_Shape_GetCenterOfMass(handle, &oldCenter);

  if (offsetCenterOfMass) {
    handle = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) handle);
  }

  // Adjust shapes
  if (JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_MutableCompound) {
    if (JPH_CompoundShape_GetNumSubShapes((JPH_CompoundShape*) handle) == 1) {
      JPH_Shape_Destroy(handle);
      handle = state.sphere->handle;
    } else {
      JPH_MutableCompoundShape_RemoveShape((JPH_MutableCompoundShape*) handle, shape->index);

      if (collider->automaticMass) {
        JPH_MutableCompoundShape_AdjustCenterOfMass((JPH_MutableCompoundShape*) handle);
      }
    }
  } else {
    handle = state.sphere->handle;
  }

  JPH_Vec3 newCenter;
  JPH_Shape_GetCenterOfMass(handle, &newCenter);

  bool hasMass = collider->automaticMass && !JPH_Shape_MustBeStatic(handle);

  // Adjust mass
  if (handle == state.sphere->handle) {
    if (collider->automaticMass) {
      JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
      adjustJoints(collider, &oldCenter, &newCenter);
    } else {
      // Maintain the center of mass, no need to adjust joints since center isn't changing
      JPH_Vec3 offset = { oldCenter.x - newCenter.x, oldCenter.y - newCenter.y, oldCenter.z - newCenter.z };
      JPH_Shape* wrapper = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, handle);
      JPH_BodyInterface_SetShape(interface, collider->id, wrapper, false, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
    }
  } else {
    if (collider->automaticMass) {
      if (offsetCenterOfMass) {
        // Replace the OffsetCenterOfMassShape with the CompoundShape
        JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
        JPH_Shape_Destroy(offsetCenterOfMass);
      } else {
        // Tell Jolt that the CompoundShape changed, recenter and recompute mass data
        JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
      }
    } else {
      // Mark shape as changed, don't update the mass/center, keep OffsetCenterOfMassShape, if any
      JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, false, JPH_Activation_DontActivate);
    }
  }

  // Remove from list, adjust shape indices
  Shape** list = &collider->shapes;
  while (*list) {
    if (*list == shape) {
      *list = shape->next;
      continue;
    } else if ((*list)->index > shape->index) {
      (*list)->index--;
    }
    list = &(*list)->next;
  }

  // Colliders with zero shapes gotta go on a special layer
  if (!collider->shapes) {
    JPH_BodyInterface_SetObjectLayer(interface, collider->id, collider->world->tagCount + 1);
  }

  lovrRelease(shape, lovrShapeDestroy);
  shape->collider = NULL;
  shape->index = ~0u;
  shape->next = NULL;
  return true;
}

static bool lovrColliderReplaceShape(Collider* collider, Shape* shape, JPH_Shape* new) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  JPH_Shape* handle = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);
  JPH_Shape* offsetCenterOfMass = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_OffsetCenterOfMass ? handle : NULL;

  if (handle == shape->handle) {
    JPH_BodyInterface_SetShape(interface, collider->id, new, collider->automaticMass, JPH_Activation_DontActivate);
    return true;
  }

  JPH_Vec3 oldCenter;
  JPH_Shape_GetCenterOfMass(handle, &oldCenter);

  if (offsetCenterOfMass) {
    JPH_Shape* inner = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) handle);

    if (inner == shape->handle) {
      JPH_Vec3 offset;
      JPH_OffsetCenterOfMassShape_GetOffset((JPH_OffsetCenterOfMassShape*) handle, &offset);
      JPH_Shape* wrapper = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, new);
      JPH_BodyInterface_SetShape(interface, collider->id, wrapper, collider->automaticMass, JPH_Activation_DontActivate);
      JPH_Shape_Destroy(handle);
      return true;
    } else {
      handle = inner;
    }
  }

  if (JPH_Shape_GetSubType(handle) != JPH_ShapeSubType_MutableCompound) {
    lovrUnreachable();
  }

  JPH_Vec3* translation = vec3_toJolt(shape->translation);
  JPH_Quat* rotation = quat_toJolt(shape->rotation);
  JPH_MutableCompoundShape_ModifyShape2((JPH_MutableCompoundShape*) handle, shape->index, translation, rotation, new);

  bool hasMass = !JPH_Shape_MustBeStatic(handle);

  if (collider->automaticMass) {
    JPH_MutableCompoundShape_AdjustCenterOfMass((JPH_MutableCompoundShape*) handle);

    JPH_Vec3 newCenter;
    JPH_Shape_GetCenterOfMass(handle, &newCenter);

    if (offsetCenterOfMass) {
      JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
      adjustJoints(collider, &oldCenter, &newCenter);
      JPH_Shape_Destroy(offsetCenterOfMass);
    } else {
      JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, hasMass, JPH_Activation_DontActivate);
      adjustJoints(collider, &oldCenter, &newCenter);
    }
  } else {
    JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, false, JPH_Activation_DontActivate);
  }

  return true;
}

static bool lovrColliderMoveShape(Collider* collider, Shape* shape, float translation[3], float rotation[4]) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  JPH_Shape* handle = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);
  JPH_Shape* offsetCenterOfMass = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_OffsetCenterOfMass ? handle : NULL;

  JPH_Vec3 oldCenter;
  JPH_Shape_GetCenterOfMass(handle, &oldCenter);

  if (offsetCenterOfMass) {
    handle = (JPH_Shape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) handle);
  }

  bool alreadyCompound = JPH_Shape_GetSubType(handle) == JPH_ShapeSubType_MutableCompound;

  // Wrap the shape in a compound shape if it isn't one already, otherwise just move the subshape
  if (alreadyCompound) {
    JPH_MutableCompoundShape_ModifyShape((JPH_MutableCompoundShape*) handle, shape->index, vec3_toJolt(translation), quat_toJolt(rotation));

    if (collider->automaticMass) {
      JPH_MutableCompoundShape_AdjustCenterOfMass((JPH_MutableCompoundShape*) handle);
    }
  } else {
    JPH_MutableCompoundShapeSettings* settings = JPH_MutableCompoundShapeSettings_Create();
    JPH_CompoundShapeSettings_AddShape2((JPH_CompoundShapeSettings*) settings, vec3_toJolt(translation), quat_toJolt(rotation), shape->handle, 0);
    handle = (JPH_Shape*) JPH_MutableCompoundShape_Create(settings);
    JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
  }

  JPH_Vec3 newCenter;
  JPH_Shape_GetCenterOfMass(handle, &newCenter);

  bool hasMass = !JPH_Shape_MustBeStatic(handle);

  if (alreadyCompound) {
    if (collider->automaticMass) {
      if (offsetCenterOfMass) {
        JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
        JPH_Shape_Destroy(offsetCenterOfMass);
      } else {
        JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, hasMass, JPH_Activation_DontActivate);
        adjustJoints(collider, &oldCenter, &newCenter);
      }
    } else {
      JPH_BodyInterface_NotifyShapeChanged(interface, collider->id, &oldCenter, false, JPH_Activation_DontActivate);
    }
  } else {
    if (collider->automaticMass) {
      // Replace the simple shape with the new compound shape, adjusting all the mass properties
      JPH_BodyInterface_SetShape(interface, collider->id, handle, hasMass, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
      adjustJoints(collider, &oldCenter, &newCenter);
    } else {
      // If automaticMass=false, use an OffsetCenterOfMassShape to keep the center of mass the same
      JPH_Vec3 offset = { oldCenter.x - newCenter.x, oldCenter.y - newCenter.y, oldCenter.z - newCenter.z };
      JPH_Shape* wrapper = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, handle);
      JPH_BodyInterface_SetShape(interface, collider->id, wrapper, false, JPH_Activation_DontActivate);
      if (offsetCenterOfMass) JPH_Shape_Destroy(offsetCenterOfMass);
    }
  }

  return true;
}

const char* lovrColliderGetTag(Collider* collider) {
  return collider->tag == 0xff ? NULL : collider->world->tags[collider->tag];
}

bool lovrColliderSetTag(Collider* collider, const char* tag) {
  collider->tag = tag ? findTag(collider->world, tag, strlen(tag)) : 0xff;
  lovrCheck(!tag || collider->tag != 0xff, "Unknown tag '%s'", tag);

  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  if (collider->tag != 0xff && collider->world->staticTagMask & (1 << collider->tag)) {
    JPH_BodyInterface_SetMotionType(interface, collider->id, JPH_MotionType_Static, JPH_Activation_DontActivate);
  }

  // Colliders without shapes go on a special object layer that doesn't collide with anything
  if (!collider->shapes) {
    return true;
  }

  JPH_ObjectLayer objectLayer = collider->tag == 0xff ? collider->world->tagCount : collider->tag;
  JPH_BodyInterface_SetObjectLayer(interface, collider->id, objectLayer);
  return true;
}

float lovrColliderGetFriction(Collider* collider) {
  return JPH_BodyInterface_GetFriction(getBodyInterface(collider, READ), collider->id);
}

bool lovrColliderSetFriction(Collider* collider, float friction) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_SetFriction(interface, collider->id, MAX(friction, 0.f));
  return !!interface;
}

float lovrColliderGetRestitution(Collider* collider) {
  return JPH_BodyInterface_GetRestitution(getBodyInterface(collider, READ), collider->id);
}

bool lovrColliderSetRestitution(Collider* collider, float restitution) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_SetRestitution(interface, collider->id, MAX(restitution, 0.f));
  return !!interface;
}

bool lovrColliderIsKinematic(Collider* collider) {
  return JPH_BodyInterface_GetMotionType(getBodyInterface(collider, READ), collider->id) != JPH_MotionType_Dynamic;
}

bool lovrColliderSetKinematic(Collider* collider, bool kinematic) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;
  bool mustBeStatic = JPH_Shape_MustBeStatic(JPH_BodyInterface_GetShape(interface, collider->id));
  bool hasStaticTag = collider->tag != 0xff && (collider->world->staticTagMask & (1 << collider->tag));
  if (!mustBeStatic && !hasStaticTag) {
    JPH_MotionType motionType = kinematic ? JPH_MotionType_Kinematic : JPH_MotionType_Dynamic;
    JPH_BodyInterface_SetMotionType(interface, collider->id, motionType, JPH_Activation_DontActivate);
  }
  return true;
}

bool lovrColliderIsSensor(Collider* collider) {
  return JPH_Body_IsSensor(collider->body);
}

void lovrColliderSetSensor(Collider* collider, bool sensor) {
  JPH_Body_SetIsSensor(collider->body, sensor);
}

bool lovrColliderIsContinuous(Collider* collider) {
  return JPH_BodyInterface_GetMotionQuality(getBodyInterface(collider, READ), collider->id) == JPH_MotionQuality_LinearCast;
}

bool lovrColliderSetContinuous(Collider* collider, bool continuous) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  JPH_MotionQuality quality = continuous ? JPH_MotionQuality_LinearCast : JPH_MotionQuality_Discrete;
  if (interface) JPH_BodyInterface_SetMotionQuality(interface, collider->id, quality);
  return !!interface;
}

float lovrColliderGetGravityScale(Collider* collider) {
  return JPH_BodyInterface_GetGravityFactor(getBodyInterface(collider, READ), collider->id);
}

bool lovrColliderSetGravityScale(Collider* collider, float scale) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_SetGravityFactor(interface, collider->id, scale);
  return !!interface;
}

bool lovrColliderIsSleepingAllowed(Collider* collider) {
  return JPH_Body_GetAllowSleeping(collider->body);
}

void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed) {
  JPH_Body_SetAllowSleeping(collider->body, allowed);
}

bool lovrColliderIsAwake(Collider* collider) {
  return JPH_BodyInterface_IsActive(getBodyInterface(collider, READ), collider->id);
}

bool lovrColliderSetAwake(Collider* collider, bool awake) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface && collider->enabled) {
    if (awake) {
      JPH_BodyInterface_ActivateBody(interface, collider->id);
    } else {
      JPH_BodyInterface_DeactivateBody(interface, collider->id);
    }
  }
  return !!interface;
}

float lovrColliderGetMass(Collider* collider) {
  if (lovrColliderIsKinematic(collider)) {
    return 0.f;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inverse mass is locked to zero
  if ((JPH_MotionProperties_GetAllowedDOFs(motion) & 0x7) == 0) {
    return 0.f;
  }

  return 1.f / JPH_MotionProperties_GetInverseMassUnchecked(motion);
}

bool lovrColliderSetMass(Collider* collider, float mass) {
  if (lovrColliderIsKinematic(collider)) {
    return true;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);

  // If all degrees of freedom are restricted, inverse mass is locked to zero
  if ((dofs & 0x7) == 0) {
    return true;
  }

  lovrCheck(mass > 0.f, "Mass must be positive");

  if (collider->automaticMass) {
    const JPH_Shape* shape = JPH_BodyInterface_GetShape(getBodyInterface(collider, READ), collider->id);
    JPH_MassProperties properties;
    JPH_Shape_GetMassProperties(shape, &properties);
    JPH_MassProperties_ScaleToMass(&properties, mass);
    JPH_MotionProperties_SetMassProperties(motion, dofs, &properties);
  } else {
    JPH_MotionProperties_SetInverseMass(motion, 1.f / mass);
  }

  return true;
}

void lovrColliderGetInertia(Collider* collider, float diagonal[3], float rotation[4]) {
  if (lovrColliderIsKinematic(collider)) {
    vec3_set(diagonal, 0.f, 0.f, 0.f);
    quat_identity(rotation);
    return;
  }

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);

  // If all degrees of freedom are restricted, inertia is locked to zero
  if ((JPH_MotionProperties_GetAllowedDOFs(motion) & 0x38) == 0) {
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
  if ((JPH_MotionProperties_GetAllowedDOFs(motion) & 0x38) == 0) {
    return;
  }

  JPH_Vec3 idiagonal = { 1.f / diagonal[0], 1.f / diagonal[1], 1.f / diagonal[2] };
  JPH_MotionProperties_SetInverseInertia(motion, &idiagonal, quat_toJolt(rotation));
}

void lovrColliderGetCenterOfMass(Collider* collider, float center[3]) {
  JPH_Vec3 centerOfMass;
  JPH_BodyInterface* interface = getBodyInterface(collider, READ);
  JPH_Shape_GetCenterOfMass(JPH_BodyInterface_GetShape(interface, collider->id), &centerOfMass);
  vec3_fromJolt(center, &centerOfMass);
}

bool lovrColliderSetCenterOfMass(Collider* collider, float center[3]) {
  if (lovrColliderIsKinematic(collider)) {
    return true;
  }

  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  const JPH_Shape* shape = JPH_BodyInterface_GetShape(interface, collider->id);

  JPH_Vec3 oldCenter;
  JPH_Shape_GetCenterOfMass(shape, &oldCenter);

  if (JPH_Shape_GetSubType(shape) == JPH_ShapeSubType_OffsetCenterOfMass) {
    const JPH_Shape* inner = JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape);
    JPH_Shape_Destroy((JPH_Shape*) shape);
    shape = inner;
  }

  JPH_Vec3 base;
  JPH_Shape_GetCenterOfMass(shape, &base);
  JPH_Vec3 offset = { center[0] - base.x, center[1] - base.y, center[2] - base.z };
  JPH_Shape* outer = (JPH_Shape*) JPH_OffsetCenterOfMassShape_Create(&offset, (JPH_Shape*) shape);
  JPH_BodyInterface_SetShape(interface, collider->id, outer, collider->automaticMass, JPH_Activation_DontActivate);
  adjustJoints(collider, &oldCenter, vec3_toJolt(center));
  return true;
}

bool lovrColliderGetAutomaticMass(Collider* collider) {
  return collider->automaticMass;
}

void lovrColliderSetAutomaticMass(Collider* collider, bool enable) {
  if (collider->automaticMass != enable) {
    collider->automaticMass = enable;

    JPH_BodyInterface* interface = getBodyInterface(collider, READ);
    JPH_Shape* shape = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);

    // While automatic mass is disabled, the compound shape's center of mass is not kept up-to-date
    // when shapes are modified.  So if automatic mass is ever re-enabled, we need to make sure to
    // refresh the center of mass, so that future shape/mass changes will use the correct value.
    if (JPH_Shape_GetSubType(shape) == JPH_ShapeSubType_MutableCompound) {
      JPH_MutableCompoundShape_AdjustCenterOfMass((JPH_MutableCompoundShape*) shape);
    }
  }
}

bool lovrColliderResetMassData(Collider* collider) {
  if (lovrColliderIsKinematic(collider)) {
    return true;
  }

  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;

  JPH_Shape* shape = (JPH_Shape*) JPH_BodyInterface_GetShape(interface, collider->id);

  if (JPH_Shape_GetSubType(shape) == JPH_ShapeSubType_OffsetCenterOfMass) {
    JPH_Vec3 oldCenter;
    JPH_Shape_GetCenterOfMass(shape, &oldCenter);

    const JPH_Shape* inner = JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape);
    JPH_BodyInterface_SetShape(interface, collider->id, inner, true, JPH_Activation_DontActivate);
    JPH_Shape_Destroy(shape);

    JPH_Vec3 newCenter;
    JPH_Shape_GetCenterOfMass(inner, &newCenter);
    adjustJoints(collider, &oldCenter, &newCenter);
  } else{
    JPH_MassProperties mass;
    JPH_Shape_GetMassProperties(shape, &mass);
    JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
    JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);
    JPH_MotionProperties_SetMassProperties(motion, dofs, &mass);
  }

  return true;
}

void lovrColliderGetDegreesOfFreedom(Collider* collider, bool translation[3], bool rotation[3]) {
  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_AllowedDOFs dofs = JPH_MotionProperties_GetAllowedDOFs(motion);
  translation[0] = dofs & JPH_AllowedDOFs_TranslationX;
  translation[1] = dofs & JPH_AllowedDOFs_TranslationY;
  translation[2] = dofs & JPH_AllowedDOFs_TranslationZ;
  rotation[0] = dofs & JPH_AllowedDOFs_RotationX;
  rotation[1] = dofs & JPH_AllowedDOFs_RotationY;
  rotation[2] = dofs & JPH_AllowedDOFs_RotationZ;
}

void lovrColliderSetDegreesOfFreedom(Collider* collider, bool translation[3], bool rotation[3]) {
  JPH_AllowedDOFs dofs = 0;

  if (translation[0]) dofs |= JPH_AllowedDOFs_TranslationX;
  if (translation[1]) dofs |= JPH_AllowedDOFs_TranslationY;
  if (translation[2]) dofs |= JPH_AllowedDOFs_TranslationZ;
  if (rotation[0]) dofs |= JPH_AllowedDOFs_RotationX;
  if (rotation[1]) dofs |= JPH_AllowedDOFs_RotationY;
  if (rotation[2]) dofs |= JPH_AllowedDOFs_RotationZ;

  JPH_MotionProperties* motion = JPH_Body_GetMotionProperties(collider->body);
  JPH_MassProperties mass;
  const JPH_Shape* shape = JPH_BodyInterface_GetShape(getBodyInterface(collider, READ), collider->id);
  JPH_Shape_GetMassProperties(shape, &mass);
  JPH_MotionProperties_SetMassProperties(motion, dofs, &mass);
}

void lovrColliderGetPosition(Collider* collider, float position[3]) {
  JPH_RVec3 p;
  JPH_BodyInterface_GetPosition(getBodyInterface(collider, READ), collider->id, &p);
  vec3_fromJolt(position, &p);
  if (collider->activeIndex != ~0u && collider->world->interpolation != 0.f) {
    vec3_lerp(position, collider->lastPosition, collider->world->interpolation);
  }
}

bool lovrColliderSetPosition(Collider* collider, float position[3]) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface_SetPosition(interface, collider->id, vec3_toJolt(position), JPH_Activation_Activate);
  vec3_init(collider->lastPosition, position);
  return true;
}

void lovrColliderGetRawPosition(Collider* collider, float position[3]) {
  JPH_RVec3 p;
  JPH_BodyInterface_GetPosition(getBodyInterface(collider, READ), collider->id, &p);
  vec3_fromJolt(position, &p);
}

void lovrColliderGetOrientation(Collider* collider, float orientation[4]) {
  JPH_Quat q;
  JPH_BodyInterface_GetRotation(getBodyInterface(collider, READ), collider->id, &q);
  quat_fromJolt(orientation, &q);
  if (collider->activeIndex != ~0u && collider->world->interpolation != 0.f) {
    quat_slerp(orientation, collider->lastOrientation, collider->world->interpolation);
  }
}

bool lovrColliderSetOrientation(Collider* collider, float orientation[4]) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface_SetRotation(interface, collider->id, quat_toJolt(orientation), JPH_Activation_Activate);
  quat_init(collider->lastOrientation, orientation);
  return true;
}

void lovrColliderGetPose(Collider* collider, float position[3], float orientation[4]) {
  JPH_RVec3 p;
  JPH_Quat q;
  JPH_BodyInterface_GetPositionAndRotation(getBodyInterface(collider, READ), collider->id, &p, &q);
  vec3_fromJolt(position, &p);
  quat_fromJolt(orientation, &q);
  if (collider->activeIndex != ~0u && collider->world->interpolation != 0.f) {
    vec3_lerp(position, collider->lastPosition, collider->world->interpolation);
    quat_slerp(orientation, collider->lastOrientation, collider->world->interpolation);
  }
}

bool lovrColliderSetPose(Collider* collider, float position[3], float orientation[4]) {
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (!interface) return false;
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface_SetPositionAndRotation(interface, collider->id, vec3_toJolt(position), quat_toJolt(orientation), JPH_Activation_Activate);
  vec3_init(collider->lastPosition, position);
  quat_init(collider->lastOrientation, orientation);
  return true;
}

bool lovrColliderMoveKinematic(Collider* collider, float position[3], float orientation[4], float dt) {
  lovrCheck(dt > 0.f, "dt must > 0");
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_MoveKinematic(interface, collider->id, vec3_toJolt(position), quat_toJolt(orientation), dt);
  return !!interface;
}

void lovrColliderGetLinearVelocity(Collider* collider, float velocity[3]) {
  JPH_Vec3 v;
  JPH_BodyInterface_GetLinearVelocity(getBodyInterface(collider, READ), collider->id, &v);
  vec3_fromJolt(velocity, &v);
}

bool lovrColliderSetLinearVelocity(Collider* collider, float velocity[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_SetLinearVelocity(interface, collider->id, vec3_toJolt(velocity));
  return !!interface;
}

void lovrColliderGetAngularVelocity(Collider* collider, float velocity[3]) {
  JPH_Vec3 v;
  JPH_BodyInterface_GetAngularVelocity(getBodyInterface(collider, READ), collider->id, &v);
  vec3_fromJolt(velocity, &v);
}

bool lovrColliderSetAngularVelocity(Collider* collider, float velocity[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_SetAngularVelocity(interface, collider->id, vec3_toJolt(velocity));
  return !!interface;
}

float lovrColliderGetLinearDamping(Collider* collider) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  return JPH_MotionProperties_GetLinearDamping(properties);
}

void lovrColliderSetLinearDamping(Collider* collider, float damping) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetLinearDamping(properties, MAX(damping, 0.f));
}

float lovrColliderGetAngularDamping(Collider* collider) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  return JPH_MotionProperties_GetAngularDamping(properties);
}

void lovrColliderSetAngularDamping(Collider* collider, float damping) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetAngularDamping(properties, MAX(damping, 0.f));
}

bool lovrColliderApplyForce(Collider* collider, float force[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddForce(interface, collider->id, vec3_toJolt(force));
  return !!interface;
}

bool lovrColliderApplyForceAtPosition(Collider* collider, float force[3], float position[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddForce2(interface, collider->id, vec3_toJolt(force), vec3_toJolt(position));
  return !!interface;
}

bool lovrColliderApplyTorque(Collider* collider, float torque[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddTorque(interface, collider->id, vec3_toJolt(torque));
  return !!interface;
}

bool lovrColliderApplyLinearImpulse(Collider* collider, float impulse[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddImpulse(interface, collider->id, vec3_toJolt(impulse));
  return !!interface;
}

bool lovrColliderApplyLinearImpulseAtPosition(Collider* collider, float impulse[3], float position[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddImpulse2(interface, collider->id, vec3_toJolt(impulse), vec3_toJolt(position));
  return !!interface;
}

bool lovrColliderApplyAngularImpulse(Collider* collider, float impulse[3]) {
  lovrCheck(collider->enabled, "Collider must be enabled");
  JPH_BodyInterface* interface = getBodyInterface(collider, WRITE);
  if (interface) JPH_BodyInterface_AddAngularImpulse(interface, collider->id, vec3_toJolt(impulse));
  return !!interface;
}

void lovrColliderGetLocalPoint(Collider* collider, float world[3], float local[3]) {
  JPH_RMatrix4x4 transform;
  JPH_BodyInterface_GetWorldTransform(getBodyInterface(collider, READ), collider->id, &transform);
  vec3_init(local, world);
  mat4_invert(&transform.m11);
  mat4_mulPoint(&transform.m11, local);
}

void lovrColliderGetWorldPoint(Collider* collider, float local[3], float world[3]) {
  JPH_RMatrix4x4 transform;
  JPH_BodyInterface_GetWorldTransform(getBodyInterface(collider, READ), collider->id, &transform);
  vec3_init(world, local);
  mat4_mulPoint(&transform.m11, world);
}

void lovrColliderGetLocalVector(Collider* collider, float world[3], float local[3]) {
  JPH_RMatrix4x4 transform;
  JPH_BodyInterface_GetWorldTransform(getBodyInterface(collider, READ), collider->id, &transform);
  vec3_init(local, world);
  mat4_invert(&transform.m11);
  mat4_mulDirection(&transform.m11, local);
}

void lovrColliderGetWorldVector(Collider* collider, float local[3], float world[3]) {
  JPH_RMatrix4x4 transform;
  JPH_BodyInterface_GetWorldTransform(getBodyInterface(collider, READ), collider->id, &transform);
  vec3_init(world, local);
  mat4_mulDirection(&transform.m11, world);
}

void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float point[3], float velocity[3]) {
  float world[3];
  lovrColliderGetWorldPoint(collider, point, world);
  lovrColliderGetLinearVelocityFromWorldPoint(collider, world, velocity);
}

void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float point[3], float velocity[3]) {
  JPH_Vec3 v;
  JPH_BodyInterface_GetPointVelocity(getBodyInterface(collider, READ), collider->id, vec3_toJolt(point), &v);
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

// Contact

void lovrContactDestroy(void* ref) {
  // Contact is a temporary object owned by the World
}

bool lovrContactIsValid(Contact* contact) {
  return thread.locked;
}

Collider* lovrContactGetColliderA(Contact* contact) {
  return contact->colliderA;
}

Collider* lovrContactGetColliderB(Contact* contact) {
  return contact->colliderB;
}

Shape* lovrContactGetShapeA(Contact* contact) {
  return subshapeToShape(contact->colliderA, JPH_ContactManifold_GetSubShapeID1(contact->manifold));
}

Shape* lovrContactGetShapeB(Contact* contact) {
  return subshapeToShape(contact->colliderB, JPH_ContactManifold_GetSubShapeID2(contact->manifold));
}

void lovrContactGetNormal(Contact* contact, float normal[3]) {
  JPH_Vec3 n;
  JPH_ContactManifold_GetWorldSpaceNormal(contact->manifold, &n);
  vec3_fromJolt(normal, &n);
}

float lovrContactGetOverlap(Contact* contact) {
  return JPH_ContactManifold_GetPenetrationDepth(contact->manifold);
}

uint32_t lovrContactGetPointCount(Contact* contact) {
  return JPH_ContactManifold_GetPointCount(contact->manifold);
}

void lovrContactGetPoint(Contact* contact, uint32_t index, float point[3]) {
  JPH_Vec3 p;
  JPH_ContactManifold_GetWorldSpaceContactPointOn2(contact->manifold, index, &p);
  vec3_fromJolt(point, &p);
}

float lovrContactGetFriction(Contact* contact) {
  return JPH_ContactSettings_GetFriction(contact->settings);
}

void lovrContactSetFriction(Contact* contact, float friction) {
  JPH_ContactSettings_SetFriction(contact->settings, MAX(friction, 0.f));
}

float lovrContactGetRestitution(Contact* contact) {
  return JPH_ContactSettings_GetRestitution(contact->settings);
}

void lovrContactSetRestitution(Contact* contact, float restitution) {
  JPH_ContactSettings_SetRestitution(contact->settings, MAX(restitution, 0.f));
}

bool lovrContactIsEnabled(Contact* contact) {
  return JPH_ContactSettings_GetIsSensor(contact->settings);
}

void lovrContactSetEnabled(Contact* contact, bool enable) {
  JPH_ContactSettings_SetIsSensor(contact->settings, !enable);
}

void lovrContactGetSurfaceVelocity(Contact* contact, float velocity[3]) {
  JPH_Vec3 v;
  JPH_ContactSettings_GetRelativeLinearSurfaceVelocity(contact->settings, &v);
  vec3_fromJolt(velocity, &v);
}

void lovrContactSetSurfaceVelocity(Contact* contact, float velocity[3]) {
  JPH_ContactSettings_SetRelativeLinearSurfaceVelocity(contact->settings, vec3_toJolt(velocity));
}

// Shapes

void lovrShapeDestroy(void* ref) {
  Shape* shape = ref;
  lovrShapeDestruct(shape);
  lovrFree(shape);
}

void lovrShapeDestruct(Shape* shape) {
  if (!shape->handle) {
    return;
  }

  if (state.freeUserData) {
    state.freeUserData(shape, shape->userdata);
  }

  if (shape->collider) {
    lovrColliderRemoveShape(shape->collider, shape);
  }

  JPH_Shape_Destroy(shape->handle);
  shape->handle = NULL;
}

bool lovrShapeIsDestroyed(Shape* shape) {
  return !shape->handle;
}

ShapeType lovrShapeGetType(Shape* shape) {
  return shape->type;
}

Collider* lovrShapeGetCollider(Shape* shape) {
  return shape->collider;
}

uintptr_t lovrShapeGetUserData(Shape* shape) {
  return shape->userdata;
}

void lovrShapeSetUserData(Shape* shape, uintptr_t userdata) {
  if (state.freeUserData) state.freeUserData(shape, shape->userdata);
  shape->userdata = userdata;
}

float lovrShapeGetVolume(Shape* shape) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    return 0.f;
  } else {
    return JPH_Shape_GetVolume(shape->handle);
  }
}

float lovrShapeGetDensity(Shape* shape) {
  if (shape->type == SHAPE_MESH || shape->type == SHAPE_TERRAIN) {
    return 0.f;
  } else {
    return JPH_ConvexShape_GetDensity((JPH_ConvexShape*) shape->handle);
  }
}

void lovrShapeSetDensity(Shape* shape, float density) {
  if (shape->type != SHAPE_MESH && shape->type != SHAPE_TERRAIN) {
    JPH_ConvexShape_SetDensity((JPH_ConvexShape*) shape->handle, density);

    if (shape->collider && shape->collider->automaticMass) {
      lovrColliderResetMassData(shape->collider);
    }
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

  JPH_MassProperties properties;
  JPH_Shape_GetMassProperties(shape->handle, &properties);

  JPH_Vec3 d;
  JPH_Matrix4x4 m;
  JPH_MassProperties_DecomposePrincipalMomentsOfInertia(&properties, &m, &d);
  vec3_fromJolt(diagonal, &d);
  quat_fromMat4(rotation, &m.m11);
}

void lovrShapeGetCenterOfMass(Shape* shape, float center[3]) {
  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);
  vec3_fromJolt(center, &centerOfMass);
}

void lovrShapeGetOffset(Shape* shape, float translation[3], float rotation[4]) {
  vec3_init(translation, shape->translation);
  quat_init(rotation, shape->rotation);
}

bool lovrShapeSetOffset(Shape* shape, float translation[3], float rotation[4]) {
  vec3_init(shape->translation, translation);
  quat_init(shape->rotation, rotation);
  return shape->collider ? lovrColliderMoveShape(shape->collider, shape, translation, rotation) : true;
}

void lovrShapeGetPose(Shape* shape, float position[3], float orientation[4]) {
  if (shape->collider) {
    float colliderPosition[3], colliderOrientation[4];
    lovrColliderGetPose(shape->collider, colliderPosition, colliderOrientation);

    if (position) {
      vec3_init(position, shape->translation);
      quat_rotate(colliderOrientation, position);
      vec3_add(position, colliderPosition);
    }

    if (orientation) quat_mul(orientation, colliderOrientation, shape->rotation);
  } else {
    if (position) vec3_init(position, shape->translation);
    if (orientation) quat_init(orientation, shape->rotation);
  }
}

void lovrShapeGetAABB(Shape* shape, float aabb[6]) {
  JPH_AABox box;
  if (shape->collider) {
    JPH_RMatrix4x4 transform;
    float position[3], orientation[4], scale[3] = { 1.f, 1.f, 1.f };
    lovrColliderGetPose(shape->collider, position, orientation);
    mat4_fromPose(&transform.m11, position, orientation);
    JPH_Shape_GetWorldSpaceBounds(shape->handle, &transform, vec3_toJolt(scale), &box);
  } else {
    JPH_Shape_GetLocalBounds(shape->handle, &box);
  }
  aabb[0] = box.min.x;
  aabb[1] = box.max.x;
  aabb[2] = box.min.y;
  aabb[3] = box.max.y;
  aabb[4] = box.min.z;
  aabb[5] = box.max.z;
}

static void inverseTransformPoint(float* point, float* position, float* orientation) {
  float inverse[4];
  quat_conjugate(quat_init(inverse, orientation));
  quat_rotate(inverse, point);
  vec3_sub(point, position);
}

static void inverseTransformRay(float* origin, float* direction, float* position, float* orientation) {
  float inverse[4];
  quat_conjugate(quat_init(inverse, orientation));
  quat_rotate(inverse, direction);
  vec3_sub(origin, position);
  quat_rotate(inverse, origin);
}

bool lovrShapeContainsPoint(Shape* shape, float point[3]) {
  float inverseRotation[4];

  if (shape->collider) {
    float position[3], orientation[4];
    lovrColliderGetPose(shape->collider, position, orientation);
    inverseTransformPoint(point, position, orientation);
  }

  inverseTransformPoint(point, shape->translation, shape->rotation);

  float center[3];
  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);
  vec3_sub(point, vec3_fromJolt(center, &centerOfMass));

  return JPH_Shape_CollidePoint(shape->handle, vec3_toJolt(point));
}

bool lovrShapeRaycast(Shape* shape, float start[3], float end[3], CastResult* hit) {
  float direction[3];
  vec3_init(direction, end);
  vec3_sub(direction, start);

  float position[3], orientation[4], inverseRotation[4];

  if (shape->collider) {
    lovrColliderGetPose(shape->collider, position, orientation);
    inverseTransformRay(start, direction, position, orientation);
  }

  inverseTransformRay(start, direction, shape->translation, shape->rotation);

  float center[3];
  JPH_Vec3 centerOfMass;
  JPH_Shape_GetCenterOfMass(shape->handle, &centerOfMass);
  vec3_sub(start, vec3_fromJolt(center, &centerOfMass));

  JPH_RayCastResult result;
  if (JPH_Shape_CastRay(shape->handle, vec3_toJolt(start), vec3_toJolt(direction), &result)) {
    vec3_init(hit->position, direction);
    vec3_scale(hit->position, result.fraction);
    vec3_add(hit->position, start);

    JPH_Vec3 normal;
    JPH_Shape_GetSurfaceNormal(shape->handle, result.subShapeID2, vec3_toJolt(hit->position), &normal);
    vec3_fromJolt(hit->normal, &normal);

    quat_rotate(shape->rotation, hit->normal);
    quat_rotate(shape->rotation, hit->position);
    vec3_add(hit->position, shape->translation);
    vec3_add(hit->position, center);

    if (shape->collider) {
      quat_rotate(orientation, hit->position);
      quat_rotate(orientation, hit->normal);
      vec3_add(hit->position, position);
    }

    return true;
  }

  return false;
}

static bool lovrShapeReplace(Shape* shape, JPH_Shape* new) {
  if (shape->collider && !lovrColliderReplaceShape(shape->collider, shape, new)) return false;
  JPH_Shape_SetUserData(new, (uint64_t) (uintptr_t) shape);
  JPH_Shape_Destroy(shape->handle);
  shape->handle = new;
  return true;
}

BoxShape* lovrBoxShapeCreate(float dimensions[3]) {
  lovrCheck(dimensions[0] > 0.f && dimensions[1] > 0.f && dimensions[2] > 0.f, "BoxShape dimensions must be positive");
  BoxShape* shape = lovrCalloc(sizeof(BoxShape));
  shape->ref = 1;
  shape->type = SHAPE_BOX;
  const JPH_Vec3 halfExtent = { dimensions[0] / 2.f, dimensions[1] / 2.f, dimensions[2] / 2.f };
  float shortestSide = MIN(dimensions[0], MIN(dimensions[1], dimensions[2]));
  float convexRadius = MIN(shortestSide * .1f, .05f);
  shape->handle = (JPH_Shape*) JPH_BoxShape_Create(&halfExtent, convexRadius);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  quat_identity(shape->rotation);
  return shape;
}

void lovrBoxShapeGetDimensions(BoxShape* shape, float dimensions[3]) {
  JPH_Vec3 halfExtent;
  JPH_BoxShape_GetHalfExtent((JPH_BoxShape*) shape->handle, &halfExtent);
  vec3_set(dimensions, halfExtent.x * 2.f, halfExtent.y * 2.f, halfExtent.z * 2.f);
}

bool lovrBoxShapeSetDimensions(BoxShape* shape, float dimensions[3]) {
  JPH_Vec3 halfExtent = { dimensions[0] / 2.f, dimensions[1] / 2.f, dimensions[2] / 2.f };
  float shortestSide = MIN(dimensions[0], MIN(dimensions[1], dimensions[2]));
  float convexRadius = MIN(shortestSide * .1f, .05f);
  return lovrShapeReplace(shape, (JPH_Shape*) JPH_BoxShape_Create(&halfExtent, convexRadius));
}

SphereShape* lovrSphereShapeCreate(float radius) {
  lovrCheck(radius > 0.f, "SphereShape radius must be positive");
  SphereShape* shape = lovrCalloc(sizeof(SphereShape));
  shape->ref = 1;
  shape->type = SHAPE_SPHERE;
  shape->handle = (JPH_Shape*) JPH_SphereShape_Create(radius);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  quat_identity(shape->rotation);
  return shape;
}

float lovrSphereShapeGetRadius(SphereShape* shape) {
  return JPH_SphereShape_GetRadius((JPH_SphereShape*) shape->handle);
}

bool lovrSphereShapeSetRadius(SphereShape* shape, float radius) {
  return lovrShapeReplace(shape, (JPH_Shape*) JPH_SphereShape_Create(radius));
}

static JPH_Shape* makeCapsule(float radius, float length) {
  float position[3];
  float orientation[4];
  vec3_set(position, 0.f, 0.f, 0.f);
  quat_fromAngleAxis(orientation, (float) M_PI / 2.f, 1.f, 0.f, 0.f);
  JPH_Shape* capsule = (JPH_Shape*) JPH_CapsuleShape_Create(length / 2.f, radius);
  JPH_Shape* wrapper = (JPH_Shape*) JPH_RotatedTranslatedShape_Create(vec3_toJolt(position), quat_toJolt(orientation), capsule);
  JPH_Shape_Destroy(capsule);
  return wrapper;
}

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CapsuleShape dimensions must be positive");
  CapsuleShape* shape = lovrCalloc(sizeof(CapsuleShape));
  shape->ref = 1;
  shape->type = SHAPE_CAPSULE;
  shape->handle = makeCapsule(radius, length);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  quat_identity(shape->rotation);
  return shape;
}

float lovrCapsuleShapeGetRadius(CapsuleShape* shape) {
  return JPH_CapsuleShape_GetRadius((JPH_CapsuleShape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape->handle));
}

bool lovrCapsuleShapeSetRadius(CapsuleShape* shape, float radius) {
  float length = lovrCapsuleShapeGetLength(shape);
  return lovrShapeReplace(shape, makeCapsule(length / 2.f, radius));
}

float lovrCapsuleShapeGetLength(CapsuleShape* shape) {
  return 2.f * JPH_CapsuleShape_GetHalfHeightOfCylinder((JPH_CapsuleShape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape->handle));
}

bool lovrCapsuleShapeSetLength(CapsuleShape* shape, float length) {
  float radius = lovrCapsuleShapeGetRadius(shape);
  return lovrShapeReplace(shape, makeCapsule(radius, length));
}

static JPH_Shape* makeCylinder(float radius, float length) {
  float position[3];
  float orientation[4];
  vec3_set(position, 0.f, 0.f, 0.f);
  quat_fromAngleAxis(orientation, (float) M_PI / 2.f, 1.f, 0.f, 0.f);
  JPH_Shape* cylinder = (JPH_Shape*) JPH_CylinderShape_Create(length / 2.f, radius);
  JPH_Shape* wrapper = (JPH_Shape*) JPH_RotatedTranslatedShape_Create(vec3_toJolt(position), quat_toJolt(orientation), cylinder);
  JPH_Shape_Destroy(cylinder);
  return wrapper;
}

CylinderShape* lovrCylinderShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CylinderShape dimensions must be positive");
  CylinderShape* shape = lovrCalloc(sizeof(CylinderShape));
  shape->ref = 1;
  shape->type = SHAPE_CYLINDER;
  shape->handle = makeCylinder(radius, length);
  JPH_Shape_SetUserData(shape->handle, (uint64_t) (uintptr_t) shape);
  quat_identity(shape->rotation);
  return shape;
}

float lovrCylinderShapeGetRadius(CylinderShape* shape) {
  return JPH_CylinderShape_GetRadius((JPH_CylinderShape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape->handle));
}

bool lovrCylinderShapeSetRadius(CylinderShape* shape, float radius) {
  float length = lovrCylinderShapeGetLength(shape);
  return lovrShapeReplace(shape, makeCylinder(radius, length));
}

float lovrCylinderShapeGetLength(CylinderShape* shape) {
  return 2.f * JPH_CylinderShape_GetHalfHeight((JPH_CylinderShape*) JPH_DecoratedShape_GetInnerShape((JPH_DecoratedShape*) shape->handle));
}

bool lovrCylinderShapeSetLength(CylinderShape* shape, float length) {
  float radius = lovrCylinderShapeGetRadius(shape);
  return lovrShapeReplace(shape, makeCylinder(radius, length));
}

ConvexShape* lovrConvexShapeCreate(float points[], uint32_t count) {
  ConvexShape* shape = lovrCalloc(sizeof(ConvexShape));
  shape->ref = 1;
  shape->type = SHAPE_CONVEX;
  JPH_ConvexHullShapeSettings* settings = JPH_ConvexHullShapeSettings_Create((const JPH_Vec3*) points, count, .05f);
  shape->handle = (JPH_Shape*) JPH_ConvexHullShapeSettings_CreateShape(settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) settings);
  quat_identity(shape->rotation);
  return shape;
}

ConvexShape* lovrConvexShapeClone(ConvexShape* parent) {
  ConvexShape* shape = lovrCalloc(sizeof(ConvexShape));
  shape->ref = 1;
  shape->type = SHAPE_CONVEX;
  shape->handle = parent->handle;
  quat_identity(shape->rotation);
  return shape;
}

uint32_t lovrConvexShapeGetPointCount(ConvexShape* shape) {
  return JPH_ConvexHullShape_GetNumPoints((JPH_ConvexHullShape*) shape->handle);
}

bool lovrConvexShapeGetPoint(ConvexShape* shape, uint32_t index, float point[3]) {
  lovrCheck(index < lovrConvexShapeGetPointCount(shape), "Invalid point index '%d'", index + 1);
  JPH_Vec3 v;
  JPH_ConvexHullShape_GetPoint((JPH_ConvexHullShape*) shape->handle, index, &v);
  vec3_fromJolt(point, &v);
  return true;
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
  quat_identity(shape->rotation);

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
  return shape;
}

MeshShape* lovrMeshShapeClone(MeshShape* parent) {
  MeshShape* shape = lovrCalloc(sizeof(MeshShape));
  shape->ref = 1;
  shape->type = SHAPE_MESH;
  shape->handle = parent->handle;
  quat_identity(shape->rotation);
  return shape;
}

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t n, float scaleXZ, float scaleY) {
  TerrainShape* shape = lovrCalloc(sizeof(TerrainShape));
  shape->ref = 1;
  shape->type = SHAPE_TERRAIN;
  quat_identity(shape->rotation);

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

// Joints

static JointNode* lovrJointGetNode(Joint* joint, Collider* collider) {
  return collider == lovrJointGetColliderA(joint) ? &joint->a : &joint->b;
}

void lovrJointInit(Joint* joint, Collider* a, Collider* b) {
  World* world = b->world;

  if (a) {
    if (a->joints) {
      joint->a.next = a->joints;
      lovrJointGetNode(a->joints, a)->prev = joint;
    }

    a->joints = joint;
  }

  if (b->joints) {
    joint->b.next = b->joints;
    lovrJointGetNode(b->joints, b)->prev = joint;
  }

  b->joints = joint;

  if (world->joints) {
    joint->world.next = world->joints;
    world->joints->world.prev = joint;
  }

  world->joints = joint;
  world->jointCount++;
}

void lovrJointDestroy(void* ref) {
  Joint* joint = ref;
  lovrJointDestruct(joint);
  lovrFree(joint);
}

void lovrJointDestruct(Joint* joint) {
  if (!joint->constraint) {
    return;
  }

  if (state.freeUserData) {
    state.freeUserData(joint, joint->userdata);
  }

  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
  World* world = b->world;
  JointNode* node;

  if (a) {
    node = &joint->a;
    if (node->next) lovrJointGetNode(node->next, a)->prev = node->prev;
    if (node->prev) lovrJointGetNode(node->prev, a)->next = node->next;
    else a->joints = node->next;
  }

  node = &joint->b;
  if (node->next) lovrJointGetNode(node->next, b)->prev = node->prev;
  if (node->prev) lovrJointGetNode(node->prev, b)->next = node->next;
  else b->joints = node->next;

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

uintptr_t lovrJointGetUserData(Joint* joint) {
  return joint->userdata;
}

void lovrJointSetUserData(Joint* joint, uintptr_t userdata) {
  if (state.freeUserData) state.freeUserData(joint, joint->userdata);
  joint->userdata = userdata;
}

void lovrJointGetAnchors(Joint* joint, float anchor1[3], float anchor2[3]) {
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
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
  World* world = b->world;

  JPH_Vec3 v;
  float force[3], x, y;
  switch (joint->type) {
    case JOINT_WELD:
      JPH_FixedConstraint_GetTotalLambdaPosition((JPH_FixedConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v)) * world->inverseDelta;
    case JOINT_BALL:
      JPH_PointConstraint_GetTotalLambdaPosition((JPH_PointConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v)) * world->inverseDelta;
    case JOINT_CONE:
      JPH_ConeConstraint_GetTotalLambdaPosition((JPH_ConeConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v)) * world->inverseDelta;
    case JOINT_DISTANCE:
      return JPH_DistanceConstraint_GetTotalLambdaPosition((JPH_DistanceConstraint*) joint->constraint);
    case JOINT_HINGE:
      JPH_HingeConstraint_GetTotalLambdaPosition((JPH_HingeConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(force, &v)) * world->inverseDelta;
    case JOINT_SLIDER:
      JPH_SliderConstraint_GetTotalLambdaPosition((JPH_SliderConstraint*) joint->constraint, &x, &y);
      return sqrtf((x * x) + (y * y)) * world->inverseDelta;
    default: lovrUnreachable();
  }
}

float lovrJointGetTorque(Joint* joint) {
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* a = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
  World* world = b->world;

  JPH_Vec3 v;
  float torque[3], x, y;
  switch (joint->type) {
    case JOINT_WELD:
      JPH_FixedConstraint_GetTotalLambdaRotation((JPH_FixedConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(torque, &v)) * world->inverseDelta;
    case JOINT_BALL:
      return 0.f;
    case JOINT_CONE:
      return JPH_ConeConstraint_GetTotalLambdaRotation((JPH_ConeConstraint*) joint->constraint);
    case JOINT_DISTANCE:
      return 0.f;
    case JOINT_HINGE:
      JPH_HingeConstraint_GetTotalLambdaRotation((JPH_HingeConstraint*) joint->constraint, &x, &y);
      return sqrtf((x * x) + (y * y)) * world->inverseDelta;
    case JOINT_SLIDER:
      JPH_SliderConstraint_GetTotalLambdaRotation((JPH_SliderConstraint*) joint->constraint, &v);
      return vec3_length(vec3_fromJolt(torque, &v)) * world->inverseDelta;
    default: lovrUnreachable();
  }
}

// WeldJoint

WeldJoint* lovrWeldJointCreate(Collider* a, Collider* b) {
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  WeldJoint* joint = lovrCalloc(sizeof(WeldJoint));
  joint->ref = 1;
  joint->type = JOINT_WELD;

  JPH_FixedConstraintSettings* settings = JPH_FixedConstraintSettings_Create();
  JPH_FixedConstraintSettings_SetAutoDetectPoint(settings, true);
  joint->constraint = (JPH_Constraint*) JPH_FixedConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

// BallJoint

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]) {
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  BallJoint* joint = lovrCalloc(sizeof(BallJoint));
  joint->ref = 1;
  joint->type = JOINT_BALL;

  JPH_PointConstraintSettings* settings = JPH_PointConstraintSettings_Create();
  JPH_PointConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_PointConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  joint->constraint = (JPH_Constraint*) JPH_PointConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

// ConeJoint

ConeJoint* lovrConeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]) {
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  lovrCheck(vec3_length(axis) > 0.f, "Cone axis can not be zero");
  vec3_normalize(axis);

  ConeJoint* joint = lovrCalloc(sizeof(ConeJoint));
  joint->ref = 1;
  joint->type = JOINT_CONE;

  JPH_ConeConstraintSettings* settings = JPH_ConeConstraintSettings_Create();
  JPH_ConeConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_ConeConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  JPH_ConeConstraintSettings_SetTwistAxis1(settings, vec3_toJolt(axis));
  JPH_ConeConstraintSettings_SetTwistAxis2(settings, vec3_toJolt(axis));
  JPH_ConeConstraintSettings_SetHalfConeAngle(settings, (float) M_PI / 4.f);
  joint->constraint = (JPH_Constraint*) JPH_ConeConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
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

bool lovrConeJointSetLimit(ConeJoint* joint, float angle) {
  lovrCheck(angle >= 0.f && angle <= (float) M_PI, "Cone joint angle limit must be between 0 and PI");
  JPH_ConeConstraint_SetHalfConeAngle((JPH_ConeConstraint*) joint->constraint, angle);
  return true;
}

// DistanceJoint

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]) {
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  DistanceJoint* joint = lovrCalloc(sizeof(DistanceJoint));
  joint->ref = 1;
  joint->type = JOINT_DISTANCE;

  JPH_DistanceConstraintSettings* settings = JPH_DistanceConstraintSettings_Create();
  JPH_DistanceConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor1));
  JPH_DistanceConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor2));
  joint->constraint = (JPH_Constraint*) JPH_DistanceConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrDistanceJointGetLimits(DistanceJoint* joint, float* min, float* max) {
  *min = JPH_DistanceConstraint_GetMinDistance((JPH_DistanceConstraint*) joint->constraint);
  *max = JPH_DistanceConstraint_GetMaxDistance((JPH_DistanceConstraint*) joint->constraint);
}

bool lovrDistanceJointSetLimits(DistanceJoint* joint, float min, float max) {
  lovrCheck(min <= max, "Distance joint lower limit can not exceed the upper limit");
  lovrCheck(max >= 0.f, "Distance joint upper limit can not be negative");
  JPH_DistanceConstraint_SetDistance((JPH_DistanceConstraint*) joint->constraint, min, max);
  return true;
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
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  lovrCheck(vec3_length(axis) > 0.f, "Hinge axis can not be zero");
  vec3_normalize(axis);

  HingeJoint* joint = lovrCalloc(sizeof(HingeJoint));
  joint->ref = 1;
  joint->type = JOINT_HINGE;

  JPH_HingeConstraintSettings* settings = JPH_HingeConstraintSettings_Create();
  JPH_HingeConstraintSettings_SetPoint1(settings, vec3_toJolt(anchor));
  JPH_HingeConstraintSettings_SetPoint2(settings, vec3_toJolt(anchor));
  JPH_HingeConstraintSettings_SetHingeAxis1(settings, vec3_toJolt(axis));
  JPH_HingeConstraintSettings_SetHingeAxis2(settings, vec3_toJolt(axis));
  joint->constraint = (JPH_Constraint*) JPH_HingeConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
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

bool lovrHingeJointSetLimits(HingeJoint* joint, float min, float max) {
  lovrCheck(min <= 0.f && min >= -(float) M_PI, "Hinge joint lower angle limit must be between -PI and 0");
  lovrCheck(max >= 0.f && max <= (float) M_PI, "Hinge joint upper angle limit must be between 0 and PI");
  JPH_HingeConstraint_SetLimits((JPH_HingeConstraint*) joint->constraint, min, max);
  return true;
}

float lovrHingeJointGetFriction(HingeJoint* joint) {
  return JPH_HingeConstraint_GetMaxFrictionTorque((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointSetFriction(HingeJoint* joint, float friction) {
  JPH_HingeConstraint_SetMaxFrictionTorque((JPH_HingeConstraint*) joint->constraint, friction);
}

MotorMode lovrHingeJointGetMotorMode(HingeJoint* joint) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  return (MotorMode) JPH_HingeConstraint_GetMotorState(constraint);
}

void lovrHingeJointSetMotorMode(HingeJoint* joint, MotorMode mode) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  JPH_HingeConstraint_SetMotorState(constraint, (JPH_MotorState) mode);
}

float lovrHingeJointGetMotorTarget(HingeJoint* joint) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  JPH_MotorState motorState = JPH_HingeConstraint_GetMotorState(constraint);
  return motorState == JPH_MotorState_Position ?
    JPH_HingeConstraint_GetTargetAngle(constraint) :
    JPH_HingeConstraint_GetTargetAngularVelocity(constraint);
}

void lovrHingeJointSetMotorTarget(HingeJoint* joint, float target) {
  JPH_HingeConstraint* constraint = (JPH_HingeConstraint*) joint->constraint;
  JPH_HingeConstraint_SetTargetAngle(constraint, target);
  JPH_HingeConstraint_SetTargetAngularVelocity(constraint, target);
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

void lovrHingeJointGetMaxMotorTorque(HingeJoint* joint, float* positive, float* negative) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  *positive = settings.maxTorqueLimit;
  *negative = -settings.minTorqueLimit;
}

void lovrHingeJointSetMaxMotorTorque(HingeJoint* joint, float positive, float negative) {
  JPH_MotorSettings settings;
  JPH_HingeConstraint_GetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
  settings.minTorqueLimit = -negative;
  settings.maxTorqueLimit = positive;
  JPH_HingeConstraint_SetMotorSettings((JPH_HingeConstraint*) joint->constraint, &settings);
}

float lovrHingeJointGetMotorTorque(HingeJoint* joint) {
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody1(constraint));
  return JPH_HingeConstraint_GetTotalLambdaMotor((JPH_HingeConstraint*) joint->constraint) * b->world->inverseDelta;
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
  lovrCheck(!a || a->world == b->world, "Joint bodies must exist in same World");
  JPH_Body* parent = a ? a->body : JPH_Body_GetFixedToWorldBody();

  SliderJoint* joint = lovrCalloc(sizeof(SliderJoint));
  joint->ref = 1;
  joint->type = JOINT_SLIDER;

  JPH_SliderConstraintSettings* settings = JPH_SliderConstraintSettings_Create();
  JPH_SliderConstraintSettings_SetSliderAxis(settings, vec3_toJolt(axis));
  joint->constraint = (JPH_Constraint*) JPH_SliderConstraintSettings_CreateConstraint(settings, parent, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(b->world->system, joint->constraint);
  lovrJointInit(joint, a, b);
  lovrRetain(joint);
  return joint;
}

void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_SliderConstraintSettings* settings = (JPH_SliderConstraintSettings*) JPH_Constraint_GetConstraintSettings((JPH_Constraint*) joint->constraint);
  JPH_SliderConstraintSettings_GetSliderAxis1(settings, &resultAxis);
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

bool lovrSliderJointSetLimits(SliderJoint* joint, float min, float max) {
  lovrCheck(min <= 0.f, "Slider joint lower distance limit can not be positive");
  lovrCheck(max >= 0.f, "Slider joint upper distance limit can not be negative");
  JPH_SliderConstraint_SetLimits((JPH_SliderConstraint*) joint->constraint, min, max);
  return true;
}

float lovrSliderJointGetFriction(SliderJoint* joint) {
  return JPH_SliderConstraint_GetMaxFrictionForce((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointSetFriction(SliderJoint* joint, float friction) {
  JPH_SliderConstraint_SetMaxFrictionForce((JPH_SliderConstraint*) joint->constraint, friction);
}

MotorMode lovrSliderJointGetMotorMode(SliderJoint* joint) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  return (MotorMode) JPH_SliderConstraint_GetMotorState(constraint);
}

void lovrSliderJointSetMotorMode(SliderJoint* joint, MotorMode mode) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  JPH_SliderConstraint_SetMotorState(constraint, (JPH_MotorState) mode);
}

float lovrSliderJointGetMotorTarget(SliderJoint* joint) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  JPH_MotorState motorState = JPH_SliderConstraint_GetMotorState(constraint);
  return motorState == JPH_MotorState_Position ?
    JPH_SliderConstraint_GetTargetPosition(constraint) :
    JPH_SliderConstraint_GetTargetVelocity(constraint);
}

void lovrSliderJointSetMotorTarget(SliderJoint* joint, float target) {
  JPH_SliderConstraint* constraint = (JPH_SliderConstraint*) joint->constraint;
  JPH_SliderConstraint_SetTargetPosition(constraint, target);
  JPH_SliderConstraint_SetTargetVelocity(constraint, target);
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
  JPH_TwoBodyConstraint* constraint = (JPH_TwoBodyConstraint*) joint->constraint;
  Collider* b = (Collider*) (uintptr_t) JPH_Body_GetUserData(JPH_TwoBodyConstraint_GetBody2(constraint));
  return JPH_SliderConstraint_GetTotalLambdaMotor((JPH_SliderConstraint*) joint->constraint) * b->world->inverseDelta;
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
