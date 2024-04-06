#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <joltc.h>

struct World {
  uint32_t ref;
  JPH_PhysicsSystem* system;
  JPH_BodyInterface* bodies;
  JPH_ObjectLayerPairFilter* objectLayerPairFilter;
  int collisionSteps;
  Collider* head;
  float defaultLinearDamping;
  float defaultAngularDamping;
  bool defaultIsSleepingAllowed;
  char* tags[MAX_TAGS];
};

struct Collider {
  uint32_t ref;
  JPH_BodyID id;
  JPH_Body* body;
  World* world;
  Collider* prev;
  Collider* next;
  arr_t(Shape*) shapes;
  arr_t(Joint*) joints;
  uint32_t tag;
};

struct Shape {
  uint32_t ref;
  ShapeType type;
  Collider* collider;
  JPH_Shape* shape;
};

struct Joint {
  uint32_t ref;
  JointType type;
  JPH_Constraint* constraint;
};

static struct {
  bool initialized;
  JPH_Shape* queryBox;
  JPH_Shape* querySphere;
  JPH_AllHit_CastShapeCollector* castShapeCollector;
} state;

// Broad phase and object phase layers
#define NUM_OP_LAYERS ((MAX_TAGS + 1) * 2)
#define NUM_BP_LAYERS 2

// UNTAGGED = 16 is mapped to object layers: 32 is kinematic untagged, and 33 is dynamic untagged
// (NO_TAG = ~0u is not used in jolt implementation)
#define UNTAGGED (MAX_TAGS)

// XXX slow, but probably fine (tag names are not on any critical path), could switch to hashing if needed
static uint32_t findTag(World* world, const char* name) {
  for (uint32_t i = 0; i < MAX_TAGS && world->tags[i]; i++) {
    if (!strcmp(world->tags[i], name)) {
      return i;
    }
  }
  return UNTAGGED;
}

bool lovrPhysicsInit(void) {
  if (state.initialized) return false;
  JPH_Init(32 * 1024 * 1024);
  state.castShapeCollector = JPH_AllHit_CastShapeCollector_Create();
  state.querySphere = (JPH_Shape*) JPH_SphereShape_Create(1.f);
  state.queryBox = (JPH_Shape*) JPH_BoxShape_Create(&(const JPH_Vec3) { .5, .5f, .5f }, 0.f);
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

  JPH_BroadPhaseLayerInterface* broadPhaseLayerInterface = JPH_BroadPhaseLayerInterfaceTable_Create(NUM_OP_LAYERS, NUM_BP_LAYERS);
  world->objectLayerPairFilter = JPH_ObjectLayerPairFilterTable_Create(NUM_OP_LAYERS);
  for (uint32_t i = 0; i < NUM_OP_LAYERS; i++) {
    for (uint32_t j = i; j < NUM_OP_LAYERS; j++) {
    JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(broadPhaseLayerInterface, i, i % 2);
      if (i % 2 == 0 && j % 2 == 0) {
        JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, i, j);
      } else {
        JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, i, j);
      }
    }
  }
  JPH_ObjectVsBroadPhaseLayerFilter* broadPhaseLayerFilter = JPH_ObjectVsBroadPhaseLayerFilterTable_Create(
    broadPhaseLayerInterface, NUM_BP_LAYERS,
    world->objectLayerPairFilter, NUM_OP_LAYERS);

  JPH_PhysicsSystemSettings settings = {
    .maxBodies = info->maxColliders,
    .maxBodyPairs = info->maxColliderPairs,
    .maxContactConstraints = info->maxContacts,
    .broadPhaseLayerInterface = broadPhaseLayerInterface,
    .objectLayerPairFilter = world->objectLayerPairFilter,
    .objectVsBroadPhaseLayerFilter = broadPhaseLayerFilter
  };
  world->system = JPH_PhysicsSystem_Create(&settings);
  world->bodies = JPH_PhysicsSystem_GetBodyInterface(world->system);

  lovrWorldSetGravity(world, info->gravity);

  for (uint32_t i = 0; i < info->tagCount; i++) {
    size_t size = strlen(info->tags[i]) + 1;
    world->tags[i] = lovrMalloc(size);
    memcpy(world->tags[i], info->tags[i], size);
  }

  return world;
}

void lovrWorldDestroy(void* ref) {
  World* world = ref;
  lovrWorldDestroyData(world);
  for (uint32_t i = 0; i < MAX_TAGS - 1 && world->tags[i]; i++) {
    lovrFree(world->tags[i]);
  }
  lovrFree(world);
}

void lovrWorldDestroyData(World* world) {
  while (world->head) {
    Collider* next = world->head->next;
    lovrColliderDestroyData(world->head);
    world->head = next;
  }
  JPH_PhysicsSystem_Destroy(world->system);
}

void lovrWorldUpdate(World* world, float dt, CollisionResolver resolver, void* userdata) {
  JPH_PhysicsSystem_Step(world->system, dt, world->collisionSteps);
}

void lovrWorldComputeOverlaps(World* world) {
  //
}

int lovrWorldGetNextOverlap(World* world, Shape** a, Shape** b) {
  return 0;
}

int lovrWorldCollide(World* world, Shape* a, Shape* b, float friction, float restitution) {
  return 0;
}

void lovrWorldGetContacts(World* world, Shape* a, Shape* b, Contact contacts[MAX_CONTACTS], uint32_t* count) {
  //
}

void lovrWorldRaycast(World* world, float x1, float y1, float z1, float x2, float y2, float z2, RaycastCallback callback, void* userdata) {
  const JPH_NarrowPhaseQuery* query = JPC_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);
  const JPH_RVec3 origin = { x1, y1, z1 };
  const JPH_Vec3 direction = { x2 - x1, y2 - y1, z2 - z1 };
  JPH_AllHit_CastRayCollector* collector = JPH_AllHit_CastRayCollector_Create();
  JPH_NarrowPhaseQuery_CastRayAll(query, &origin, &direction, collector, NULL, NULL, NULL);
  size_t hit_count;
  JPH_RayCastResult* hit_array = JPH_AllHit_CastRayCollector_GetHits(collector, &hit_count);
  for (int i = 0; i < hit_count; i++) {
    float x = x1 + hit_array[i].fraction * (x2 - x1);
    float y = y1 + hit_array[i].fraction * (y2 - y1);
    float z = z1 + hit_array[i].fraction * (z2 - z1);
    // todo: assuming one shape per collider; doesn't support compound shape
    Collider* collider = (Collider*) JPH_BodyInterface_GetUserData(
      world->bodies,
      hit_array[i].bodyID);
    size_t count;
    Shape** shape = lovrColliderGetShapes(collider, &count);
    const JPH_RVec3 position = { x, y, z };
    JPH_Vec3 normal;
    JPH_Body_GetWorldSpaceSurfaceNormal(collider->body, hit_array[i].subShapeID2, &position, &normal);

    bool shouldStop = callback(
      shape[0], // assumes one shape per collider; todo: compound shapes
      x, y, z,
      normal.x, normal.y, normal.z,
      userdata);

    if (shouldStop) {
      break;
    }
  }
  JPH_AllHit_CastRayCollector_Destroy(collector);
}

static bool lovrWorldQueryShape(World* world, JPH_Shape* shape, float position[3], float scale[3], QueryCallback callback, void* userdata) {
  JPH_RMatrix4x4 transform;
  float* m = &transform.m11;
  mat4_identity(m);
  mat4_translate(m, position[0], position[1], position[2]);
  mat4_scale(m, scale[0], scale[1], scale[2]);

  JPH_Vec3 direction = { 0.f };
  JPH_RVec3 base_offset = { 0.f };
  const JPH_NarrowPhaseQuery* query = JPC_PhysicsSystem_GetNarrowPhaseQueryNoLock(world->system);
  JPH_AllHit_CastShapeCollector_Reset(state.castShapeCollector);
  JPH_NarrowPhaseQuery_CastShape(query, shape, &transform, &direction, &base_offset, state.castShapeCollector);
  size_t hit_count;
  JPH_ShapeCastResult* hit_array = JPH_AllHit_CastShapeCollector_GetHits(state.castShapeCollector, &hit_count);
  for (int i = 0; i < hit_count; i++) {
    JPH_BodyID id = JPH_AllHit_CastShapeCollector_GetBodyID2(state.castShapeCollector, i);
    Collider* collider = (Collider*) JPH_BodyInterface_GetUserData(
      world->bodies,
      id);
    size_t count;
    Shape** shape = lovrColliderGetShapes(collider, &count);
    bool shouldStop = callback(shape[0], userdata);
    if (shouldStop) {
      break;
    }
  }
  return hit_count > 0;
}

bool lovrWorldQueryBox(World* world, float position[3], float size[3], QueryCallback callback, void* userdata) {
  return lovrWorldQueryShape(world, state.queryBox, position, size, callback, userdata);
}

bool lovrWorldQuerySphere(World* world, float position[3], float radius, QueryCallback callback, void* userdata) {
  float scale[3] = { radius, radius, radius };
  return lovrWorldQueryShape(world, state.querySphere, position, scale, callback, userdata);
}

Collider* lovrWorldGetFirstCollider(World* world) {
  return world->head;
}

void lovrWorldGetGravity(World* world, float gravity[3]) {
  JPH_Vec3 g;
  JPH_PhysicsSystem_GetGravity(world->system, &g);
  vec3_init(gravity, &g.x);
}

void lovrWorldSetGravity(World* world, float gravity[3]) {
  JPH_PhysicsSystem_SetGravity(world->system, &(const JPH_Vec3) { gravity[0], gravity[1], gravity[2] });
}

const char* lovrWorldGetTagName(World* world, uint32_t tag) {
  return (tag == UNTAGGED) ? NULL : world->tags[tag];
}

void lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == UNTAGGED || j == UNTAGGED) {
    return;
  }
  uint32_t iStatic = i * 2;
  uint32_t jStatic = j * 2;
  uint32_t iDynamic = i * 2 + 1;
  uint32_t jDynamic = j * 2 + 1;
  JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, iDynamic, jDynamic);
  JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, iDynamic, jStatic);
  JPH_ObjectLayerPairFilterTable_DisableCollision(world->objectLayerPairFilter, iStatic, jDynamic);
}

void lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == UNTAGGED || j == UNTAGGED) {
    return;
  }
  uint32_t iStatic = i * 2;
  uint32_t jStatic = j * 2;
  uint32_t iDynamic = i * 2 + 1;
  uint32_t jDynamic = j * 2 + 1;
  JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, iDynamic, jDynamic);
  JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, iDynamic, jStatic);
  JPH_ObjectLayerPairFilterTable_EnableCollision(world->objectLayerPairFilter, iStatic, jDynamic);
}

bool lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2) {
  uint32_t i = findTag(world, tag1);
  uint32_t j = findTag(world, tag2);
  if (i == UNTAGGED || j == UNTAGGED) {
    return true;
  }
  uint32_t iDynamic = i * 2 + 1;
  uint32_t jDynamic = j * 2 + 1;
  return JPH_ObjectLayerPairFilterTable_ShouldCollide(world->objectLayerPairFilter, iDynamic, jDynamic);
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

Collider* lovrColliderCreate(World* world, float x, float y, float z) {
  // todo: crashes when too many are added
  Collider* collider = lovrCalloc(sizeof(Collider));
  collider->ref = 1;
  collider->world = world;
  collider->tag = UNTAGGED;
  JPH_MotionType motionType = JPH_MotionType_Dynamic;
  JPH_ObjectLayer objectLayer = UNTAGGED * 2 + 1;

  const JPH_RVec3 position = { x, y, z };
  const JPH_Quat rotation = { 0.f, 0.f, 0.f, 1.f };
  // todo: a placeholder querySphere shape is used in collider, then replaced in lovrColliderAddShape
  JPH_BodyCreationSettings* settings = JPH_BodyCreationSettings_Create3(
    state.querySphere, &position, &rotation, motionType, objectLayer);
  collider->body = JPH_BodyInterface_CreateBody(world->bodies, settings);
  JPH_BodyCreationSettings_Destroy(settings);
  collider->id = JPH_Body_GetID(collider->body);
  JPH_BodyInterface_AddBody(world->bodies, collider->id, JPH_Activation_Activate);
  JPH_BodyInterface_SetUserData(world->bodies, collider->id, (uint64_t) collider);
  lovrColliderSetLinearDamping(collider, world->defaultLinearDamping, 0.f);
  lovrColliderSetAngularDamping(collider, world->defaultAngularDamping, 0.f);
  lovrColliderSetSleepingAllowed(collider, world->defaultIsSleepingAllowed);

  arr_init(&collider->shapes);
  arr_init(&collider->joints);

  // Adjust the world's collider list
  if (!world->head) {
    world->head = collider;
  } else {
    collider->next = world->head;
    collider->next->prev = collider;
    world->head = collider;
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
  lovrFree(collider);
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

  JPH_BodyInterface_RemoveBody(collider->world->bodies, collider->id);
  collider->body = NULL;

  if (collider->next) collider->next->prev = collider->prev;
  if (collider->prev) collider->prev->next = collider->next;
  if (collider->world->head == collider) collider->world->head = collider->next;
  collider->next = collider->prev = NULL;

  // If the Collider is destroyed, the world lets go of its reference to this Collider
  lovrRelease(collider, lovrColliderDestroy);
}

bool lovrColliderIsDestroyed(Collider* collider) {
  return !collider->body;
}

void lovrColliderInitInertia(Collider* collider, Shape* shape) {
  //
}

World* lovrColliderGetWorld(Collider* collider) {
  return collider->world;
}

Collider* lovrColliderGetNext(Collider* collider) {
  return collider->next;
}

void lovrColliderAddShape(Collider* collider, Shape* shape) {
  lovrRetain(shape);
  shape->collider = collider;
  arr_push(&collider->shapes, shape);
  bool isMeshOrTerrain = (shape->type == SHAPE_TERRAIN) || (shape->type == SHAPE_MESH);
  bool shouldUpdateMass = !isMeshOrTerrain;
  if (isMeshOrTerrain) {
    lovrColliderSetKinematic(shape->collider, true);
  }
  JPH_BodyInterface_SetShape(collider->world->bodies, collider->id, shape->shape, shouldUpdateMass, JPH_Activation_Activate);
}

void lovrColliderRemoveShape(Collider* collider, Shape* shape) {
  if (shape->collider == collider) {
    // todo: actions necessary for compound shapes
    shape->collider = NULL;
    lovrRelease(shape, lovrShapeDestroy);
  }
}

Shape** lovrColliderGetShapes(Collider* collider, size_t* count) {
  *count = collider->shapes.length;
  return collider->shapes.data;
}

Joint** lovrColliderGetJoints(Collider* collider, size_t* count) {
  *count = collider->joints.length;
  return collider->joints.data;
}

const char* lovrColliderGetTag(Collider* collider) {
  return lovrWorldGetTagName(collider->world, collider->tag);
}

bool lovrColliderSetTag(Collider* collider, const char* tag) {
  if (!tag) {
    collider->tag = UNTAGGED;
  } else {
    collider->tag = findTag(collider->world, tag);
    if (collider->tag == UNTAGGED) {
      return false;
    }
  }
  bool kinematic = lovrColliderIsKinematic(collider);
  JPH_ObjectLayer objectLayer = collider->tag * 2 + (kinematic ? 0 : 1);
  JPH_BodyInterface_SetObjectLayer(collider->world->bodies, collider->id, objectLayer);
  return true;
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
  JPH_ObjectLayer objectLayer = JPH_BodyInterface_GetObjectLayer(collider->world->bodies, collider->id);
  return objectLayer % 2 == 0;
}

void lovrColliderSetKinematic(Collider* collider, bool kinematic) {
  JPH_ObjectLayer objectLayer = collider->tag * 2 + (kinematic ? 0 : 1);
  JPH_BodyInterface_SetObjectLayer(collider->world->bodies, collider->id, objectLayer);
  if (kinematic) {
    JPH_BodyInterface_DeactivateBody(collider->world->bodies, collider->id);
    JPH_BodyInterface_SetMotionType(
      collider->world->bodies,
      collider->id,
      JPH_MotionType_Kinematic,
      JPH_Activation_DontActivate);
  } else {
    JPH_BodyInterface_SetMotionType(
      collider->world->bodies,
      collider->id,
      JPH_MotionType_Dynamic,
      JPH_Activation_Activate);
  }
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
   if (collider->shapes.length > 0) {
    JPH_MotionProperties* motionProperties = JPH_Body_GetMotionProperties(collider->body);
    return 1.f / JPH_MotionProperties_GetInverseMassUnchecked(motionProperties);
  }
  return 0.f;
}

void lovrColliderSetMass(Collider* collider, float mass) {
  if (collider->shapes.length > 0) {
    JPH_MotionProperties* motionProperties = JPH_Body_GetMotionProperties(collider->body);
    Shape* shape = collider->shapes.data[0];
    JPH_MassProperties* massProperties;
    JPH_Shape_GetMassProperties(shape->shape, massProperties);
    JPH_MassProperties_ScaleToMass(massProperties, mass);
    JPH_MotionProperties_SetMassProperties(motionProperties, JPH_AllowedDOFs_All, massProperties);
  }
}

void lovrColliderGetMassData(Collider* collider, float* cx, float* cy, float* cz, float* mass, float inertia[6]) {
  //
}

void lovrColliderSetMassData(Collider* collider, float cx, float cy, float cz, float mass, float inertia[6]) {
  //
}

void lovrColliderGetPosition(Collider* collider, float* x, float* y, float* z) {
  JPH_RVec3 position;
  JPH_Body_GetPosition(collider->body, &position);
  *x = position.x;
  *y = position.y;
  *z = position.z;
}

void lovrColliderSetPosition(Collider* collider, float x, float y, float z) {
  JPH_RVec3 position = { x, y, z };
  JPH_BodyInterface_SetPosition(collider->world->bodies, collider->id, &position, JPH_Activation_Activate);
}

void lovrColliderGetOrientation(Collider* collider, float* orientation) {
  JPH_Quat quat;
  JPH_Body_GetRotation(collider->body, &quat);
  orientation[0] = quat.x;
  orientation[1] = quat.y;
  orientation[2] = quat.z;
  orientation[3] = quat.w;
}

void lovrColliderSetOrientation(Collider* collider, float* orientation) {
  JPH_Quat rotation = {
   .x = orientation[0],
   .y = orientation[1],
   .z = orientation[2],
   .w = orientation[3]
 };
  JPH_BodyInterface_SetRotation(
    collider->world->bodies,
    collider->id,
    &rotation,
    JPH_Activation_Activate);
}

void lovrColliderGetLinearVelocity(Collider* collider, float* x, float* y, float* z) {
  JPH_Vec3 velocity;
  JPH_BodyInterface_GetLinearVelocity(collider->world->bodies, collider->id, &velocity);
  *x = velocity.x;
  *y = velocity.y;
  *z = velocity.z;
}

void lovrColliderSetLinearVelocity(Collider* collider, float x, float y, float z) {
  const JPH_Vec3 velocity = { x, y, z };
  JPH_BodyInterface_SetLinearVelocity(collider->world->bodies, collider->id, &velocity);
}

void lovrColliderGetAngularVelocity(Collider* collider, float* x, float* y, float* z) {
  JPH_Vec3 velocity;
  JPH_BodyInterface_GetAngularVelocity(collider->world->bodies, collider->id, &velocity);
  *x = velocity.x;
  *y = velocity.y;
  *z = velocity.z;
}

void lovrColliderSetAngularVelocity(Collider* collider, float x, float y, float z) {
  JPH_Vec3 velocity = { x, y, z };
  JPH_BodyInterface_SetAngularVelocity(collider->world->bodies, collider->id, &velocity);
}

void lovrColliderGetLinearDamping(Collider* collider, float* damping, float* threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  *damping = JPH_MotionProperties_GetLinearDamping(properties);
  *threshold = 0.f;
}

void lovrColliderSetLinearDamping(Collider* collider, float damping, float threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetLinearDamping(properties, damping);
  if (threshold != 0.f) {
    lovrLog(LOG_WARN, "PHY", "Jolt does not support velocity threshold parameter for damping");
  }
}

void lovrColliderGetAngularDamping(Collider* collider, float* damping, float* threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  *damping = JPH_MotionProperties_GetAngularDamping(properties);
  *threshold = 0.f;
}

void lovrColliderSetAngularDamping(Collider* collider, float damping, float threshold) {
  JPH_MotionProperties* properties = JPH_Body_GetMotionProperties(collider->body);
  JPH_MotionProperties_SetAngularDamping(properties, damping);
  if (threshold != 0.f) {
    lovrLog(LOG_WARN, "PHY", "Jolt does not support velocity threshold parameter for damping");
  }
}

void lovrColliderApplyForce(Collider* collider, float x, float y, float z) {
  JPH_Vec3 force = { x, y, z };
  JPH_BodyInterface_AddForce(collider->world->bodies, collider->id, &force);
}

void lovrColliderApplyForceAtPosition(Collider* collider, float x, float y, float z, float cx, float cy, float cz) {
  JPH_Vec3 force = { x, y, z };
  JPH_RVec3 position = { cx, cy, cz };
  JPH_BodyInterface_AddForce2(collider->world->bodies, collider->id, &force, &position);
}

void lovrColliderApplyTorque(Collider* collider, float x, float y, float z) {
  JPH_Vec3 torque = { x, y, z };
  JPH_BodyInterface_AddTorque(collider->world->bodies, collider->id, &torque);
}

void lovrColliderApplyLinearImpulse(Collider* collider, float impulse[3]) {
  JPH_Vec3 vector = { impulse[0], impulse[1], impulse[2] };
  JPH_BodyInterface_AddImpulse(collider->world->bodies, collider->id, &vector);
}

void lovrColliderApplyLinearImpulseAtPosition(Collider* collider, float impulse[3], float position[3]) {
  JPH_Vec3 vector = { impulse[0], impulse[1], impulse[2] };
  JPH_Vec3 point = { position[0], position[1], position[2] };
  JPH_BodyInterface_AddImpulse2(collider->world->bodies, collider->id, &vector, &point);
}

void lovrColliderApplyAngularImpulse(Collider* collider, float impulse[3]) {
  JPH_Vec3 vector = { impulse[0], impulse[1], impulse[2] };
  JPH_BodyInterface_AddAngularImpulse(collider->world->bodies, collider->id, &vector);
}

void lovrColliderGetLocalCenter(Collider* collider, float* x, float* y, float* z) {
  // todo: applicable for CompoundShape and OffsetCenterOfMassShape
  *x = 0.f;
  *y = 0.f;
  *z = 0.f;
}

void lovrColliderGetLocalPoint(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z) {
  float position[4] = { wx, wy, wz, 1.f };
  JPH_RMatrix4x4 transform;
  float* m = &transform.m11;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  mat4_invert(m);
  mat4_mulVec4(m, position);
  *x = position[0];
  *y = position[1];
  *z = position[2];
}

void lovrColliderGetWorldPoint(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz) {
  float position[4] = { x, y, z, 1.f };
  JPH_RMatrix4x4 transform;
  float* m = &transform.m11;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  mat4_mulVec4(m, position);
  *wx = position[0];
  *wy = position[1];
  *wz = position[2];
}

void lovrColliderGetLocalVector(Collider* collider, float wx, float wy, float wz, float* x, float* y, float* z) {
  float direction[4] = { wx, wy, wz, 0.f };
  JPH_RMatrix4x4 transform;
  float* m = &transform.m11;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  mat4_invert(m);
  mat4_mulVec4(m, direction);
  *x = direction[0];
  *y = direction[1];
  *z = direction[2];
}

void lovrColliderGetWorldVector(Collider* collider, float x, float y, float z, float* wx, float* wy, float* wz) {
  float direction[4] = { x, y, z, 0.f };
  JPH_RMatrix4x4 transform;
  float* m = &transform.m11;
  JPH_Body_GetWorldTransform(collider->body, &transform);
  mat4_mulVec4(m, direction);
  *wx = direction[0];
  *wy = direction[1];
  *wz = direction[2];
}

void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float x, float y, float z, float* vx, float* vy, float* vz) {
  float wx, wy, wz;
  lovrColliderGetWorldPoint(collider, x, y, z, &wx, &wy, &wz);
  lovrColliderGetLinearVelocityFromWorldPoint(collider, wx, wy, wz, vx, vy, vz);
}

void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float wx, float wy, float wz, float* vx, float* vy, float* vz) {
  JPH_RVec3 point = { wx, wy, wz };
  JPH_Vec3 velocity;
  JPH_BodyInterface_GetPointVelocity(collider->world->bodies, collider->id, &point, &velocity);
  *vx = velocity.x;
  *vy = velocity.y;
  *vz = velocity.z;
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

void lovrShapeDestroy(void* ref) {
  Shape* shape = ref;
  lovrShapeDestroyData(shape);
  lovrFree(shape);
}

void lovrShapeDestroyData(Shape* shape) {
  if (shape->shape) {
    JPH_Shape_Destroy(shape->shape);
    shape->shape = NULL;
  }
}

ShapeType lovrShapeGetType(Shape* shape) {
  return shape->type;
}

Collider* lovrShapeGetCollider(Shape* shape) {
  return shape->collider;
}

bool lovrShapeIsEnabled(Shape* shape) {
  return true;
}

void lovrShapeSetEnabled(Shape* shape, bool enabled) {
  if (!enabled) {
    lovrLog(LOG_WARN, "PHY", "Jolt doesn't support disabling shapes");
  }
}

bool lovrShapeIsSensor(Shape* shape) {
  lovrLog(LOG_WARN, "PHY", "Jolt sensor property fetched from collider, not shape");
  return JPH_Body_IsSensor(shape->collider->body);
}

void lovrShapeSetSensor(Shape* shape, bool sensor) {
  lovrLog(LOG_WARN, "PHY", "Jolt sensor property is applied to collider, not shape");
  JPH_Body_SetIsSensor(shape->collider->body, sensor);
}

void lovrShapeGetPosition(Shape* shape, float* x, float* y, float* z) {
  // todo: compound shapes
  *x = 0.f;
  *y = 0.f;
  *z = 0.f;
}

void lovrShapeSetPosition(Shape* shape, float x, float y, float z) {
  // todo: compound shapes
}

void lovrShapeGetOrientation(Shape* shape, float* orientation) {
  // todo: compound shapes
  orientation[0] = 0.f;
  orientation[1] = 0.f;
  orientation[2] = 0.f;
  orientation[3] = 1.f;
}

void lovrShapeSetOrientation(Shape* shape, float* orientation) {
  // todo: compound shapes
}

void lovrShapeGetMass(Shape* shape, float density, float* cx, float* cy, float* cz, float* mass, float inertia[6]) {}

void lovrShapeGetAABB(Shape* shape, float aabb[6]) {
  // todo: with compound shapes this is no longer correct
  lovrColliderGetAABB(shape->collider, aabb);
}

SphereShape* lovrSphereShapeCreate(float radius) {
  lovrCheck(radius > 0.f, "SphereShape radius must be positive");
  SphereShape* sphere = lovrCalloc(sizeof(SphereShape));
  sphere->ref = 1;
  sphere->type = SHAPE_SPHERE;
  sphere->shape = (JPH_Shape*) JPH_SphereShape_Create(radius);
  return sphere;
}

float lovrSphereShapeGetRadius(SphereShape* sphere) {
  return JPH_SphereShape_GetRadius((JPH_SphereShape*) sphere->shape);
}

void lovrSphereShapeSetRadius(SphereShape* sphere, float radius) {
  lovrLog(LOG_WARN, "PHY", "Jolt SphereShape radius is read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

BoxShape* lovrBoxShapeCreate(float w, float h, float d) {
  BoxShape* box = lovrCalloc(sizeof(BoxShape));
  box->ref = 1;
  box->type = SHAPE_BOX;
  const JPH_Vec3 halfExtent = { w / 2.f, h / 2.f, d / 2.f };
  box->shape = (JPH_Shape*) JPH_BoxShape_Create(&halfExtent, 0.f);
  return box;
}

void lovrBoxShapeGetDimensions(BoxShape* box, float* w, float* h, float* d) {
  JPH_Vec3 halfExtent;
  JPH_BoxShape_GetHalfExtent((JPH_BoxShape*) box->shape, &halfExtent);
  *w = halfExtent.x * 2.f;
  *h = halfExtent.y * 2.f;
  *d = halfExtent.z * 2.f;
}

void lovrBoxShapeSetDimensions(BoxShape* box, float w, float h, float d) {
  lovrLog(LOG_WARN, "PHY", "Jolt BoxShape dimensions are read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CapsuleShape dimensions must be positive");
  CapsuleShape* capsule = lovrCalloc(sizeof(CapsuleShape));
  capsule->ref = 1;
  capsule->type = SHAPE_CAPSULE;
  capsule->shape = (JPH_Shape*) JPH_CapsuleShape_Create(length / 2, radius);
  return capsule;
}

float lovrCapsuleShapeGetRadius(CapsuleShape* capsule) {
  return JPH_CapsuleShape_GetRadius((JPH_CapsuleShape*) capsule->shape);
}

void lovrCapsuleShapeSetRadius(CapsuleShape* capsule, float radius) {
  lovrLog(LOG_WARN, "PHY", "Jolt CapsuleShape radius is read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

float lovrCapsuleShapeGetLength(CapsuleShape* capsule) {
  return 2.f * JPH_CapsuleShape_GetHalfHeightOfCylinder((JPH_CapsuleShape*) capsule->shape);
}

void lovrCapsuleShapeSetLength(CapsuleShape* capsule, float length) {
  lovrLog(LOG_WARN, "PHY", "Jolt CapsuleShape length is read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

CylinderShape* lovrCylinderShapeCreate(float radius, float length) {
  lovrCheck(radius > 0.f && length > 0.f, "CylinderShape dimensions must be positive");
  CylinderShape* Cylinder = lovrCalloc(sizeof(CylinderShape));
  Cylinder->ref = 1;
  Cylinder->type = SHAPE_CYLINDER;
  Cylinder->shape = (JPH_Shape*) JPH_CylinderShape_Create(length / 2.f, radius);
  return Cylinder;
}

float lovrCylinderShapeGetRadius(CylinderShape* Cylinder) {
  return JPH_CylinderShape_GetRadius((JPH_CylinderShape*) Cylinder->shape);
}

void lovrCylinderShapeSetRadius(CylinderShape* Cylinder, float radius) {
  lovrLog(LOG_WARN, "PHY", "Jolt CylinderShape radius is read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

float lovrCylinderShapeGetLength(CylinderShape* Cylinder) {
  return JPH_CylinderShape_GetHalfHeight((JPH_CylinderShape*) Cylinder->shape) * 2.f;
}

void lovrCylinderShapeSetLength(CylinderShape* cylinder, float length) {
  lovrLog(LOG_WARN, "PHY", "Jolt CylinderShape length is read-only");
  // todo: no setter available, but the shape could be removed and re-added
}

MeshShape* lovrMeshShapeCreate(int vertexCount, float vertices[], int indexCount, uint32_t indices[]) {
  MeshShape* mesh = lovrCalloc(sizeof(MeshShape));
  mesh->ref = 1;
  mesh->type = SHAPE_MESH;

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
  mesh->shape = (JPH_Shape*) JPH_MeshShapeSettings_CreateShape(shape_settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) shape_settings);
  lovrFree(indexedTriangles);
  // Note that we're responsible for freeing the vertices/indices when we're done with them because
  // ODE took ownership of mesh data.  If ODE gets removed, we should probably get rid of this and
  // have the caller free the vertices/indices themselves.
  lovrFree(vertices);
  lovrFree(indices);
  return mesh;
}

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t widthSamples, uint32_t depthSamples, float horizontalScale, float verticalScale) {
  lovrCheck(widthSamples == depthSamples, "Jolt needs terrain width and depth to be the same");
  TerrainShape* terrain = lovrCalloc(sizeof(TerrainShape));
  terrain->ref = 1;
  terrain->type = SHAPE_TERRAIN;
  const JPH_Vec3 offset = {
    .x = -.5f * horizontalScale,
    .y = 0.f,
    .z = -.5f * horizontalScale
  };
  const JPH_Vec3 scale = {
    .x = horizontalScale / widthSamples,
    .y = verticalScale,
    .z = horizontalScale / depthSamples
  };

  JPH_HeightFieldShapeSettings* shape_settings = JPH_HeightFieldShapeSettings_Create(
    vertices, &offset, &scale, widthSamples);
  terrain->shape = (JPH_Shape*) JPH_HeightFieldShapeSettings_CreateShape(shape_settings);
  JPH_ShapeSettings_Destroy((JPH_ShapeSettings*) shape_settings);
  return terrain;
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

void lovrJointDestroy(void* ref) {
  Joint* joint = ref;
  lovrJointDestroyData(joint);
  lovrFree(joint);
}

void lovrJointDestroyData(Joint* joint) {
  if (!joint->constraint) {
    return;
  }

  JPH_PhysicsSystem* system;
  JPH_Body* bodyA = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  if (bodyA) {
    Collider* collider = (Collider*) JPH_Body_GetUserData(bodyA);
    system = collider->world->system;
    for (size_t i = 0; i < collider->joints.length; i++) {
      if (collider->joints.data[i] == joint) {
        arr_splice(&collider->joints, i, 1);
        break;
      }
    }
  }

  JPH_Body* bodyB = JPH_TwoBodyConstraint_GetBody2((JPH_TwoBodyConstraint*) joint->constraint);
  if (bodyB) {
    Collider* collider = (Collider*) JPH_Body_GetUserData(bodyB);
    for (size_t i = 0; i < collider->joints.length; i++) {
      if (collider->joints.data[i] == joint) {
        arr_splice(&collider->joints, i, 1);
        break;
      }
    }
  }
  if (system) {
    JPH_PhysicsSystem_RemoveConstraint(system, joint->constraint);
  }
  joint->constraint = NULL;
  lovrRelease(joint, lovrJointDestroy);
}

JointType lovrJointGetType(Joint* joint) {
  return joint->type;
}

void lovrJointGetColliders(Joint* joint, Collider** a, Collider** b) {
  JPH_Body* bodyA = JPH_TwoBodyConstraint_GetBody1((JPH_TwoBodyConstraint*) joint->constraint);
  JPH_Body* bodyB = JPH_TwoBodyConstraint_GetBody2((JPH_TwoBodyConstraint*) joint->constraint);

  if (bodyA) {
    *a = (Collider*) JPH_Body_GetUserData(bodyA);
  }

  if (bodyB) {
    *b = (Collider*) JPH_Body_GetUserData(bodyB);
  }
}

bool lovrJointIsEnabled(Joint* joint) {
  return JPH_Constraint_GetEnabled(joint->constraint);
}

void lovrJointSetEnabled(Joint* joint, bool enable) {
  JPH_Constraint_SetEnabled(joint->constraint, enable);
}

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]) {
  lovrCheck(a->world == b->world, "Joint bodies must exist in same World");
  BallJoint* joint = lovrCalloc(sizeof(BallJoint));
  joint->ref = 1;
  joint->type = JOINT_BALL;

  JPH_PointConstraintSettings* settings = JPH_PointConstraintSettings_Create();
  JPH_RVec3 point1 = { anchor[0], anchor[1], anchor[2] };
  JPH_RVec3 point2 = { anchor[0], anchor[1], anchor[2] };
  JPH_PointConstraintSettings_SetPoint1(settings, &point1);
  JPH_PointConstraintSettings_SetPoint2(settings, &point2);
  joint->constraint = (JPH_Constraint*) JPH_PointConstraintSettings_CreateConstraint(settings, a->body, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(a->world->system, joint->constraint);
  arr_push(&a->joints, joint);
  arr_push(&b->joints, joint);
  lovrRetain(joint);
  return joint;
}

void lovrBallJointGetAnchors(BallJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

void lovrBallJointSetAnchor(BallJoint* joint, float anchor[3]) {
  JPH_RVec3 point;
  point.x = anchor[0];
  point.y = anchor[1];
  point.z = anchor[2];
  JPH_PointConstraint_SetPoint1((JPH_PointConstraint*) joint->constraint, JPH_ConstraintSpace_WorldSpace, &point);
  JPH_PointConstraint_SetPoint2((JPH_PointConstraint*) joint->constraint, JPH_ConstraintSpace_WorldSpace, &point);
}

float lovrBallJointGetResponseTime(Joint* joint) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support BallJoint response time");
}

void lovrBallJointSetResponseTime(Joint* joint, float responseTime) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support BallJoint response time");
}

float lovrBallJointGetTightness(Joint* joint) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support BallJoint tightness");
}

void lovrBallJointSetTightness(Joint* joint, float tightness) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support BallJoint tightness");
}

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]) {
  lovrCheck(a->world == b->world, "Joint bodies must exist in same World");
  DistanceJoint* joint = lovrCalloc(sizeof(DistanceJoint));
  joint->ref = 1;
  joint->type = JOINT_DISTANCE;

  JPH_DistanceConstraintSettings* settings = JPH_DistanceConstraintSettings_Create();
  JPH_RVec3 point1 = { anchor1[0], anchor1[1], anchor1[2] };
  JPH_RVec3 point2 = { anchor2[0], anchor2[1], anchor2[2] };
  JPH_DistanceConstraintSettings_SetPoint1(settings, &point1);
  JPH_DistanceConstraintSettings_SetPoint2(settings, &point2);
  joint->constraint = (JPH_Constraint*) JPH_DistanceConstraintSettings_CreateConstraint(settings, a->body, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(a->world->system, joint->constraint);
  arr_push(&a->joints, joint);
  arr_push(&b->joints, joint);
  lovrRetain(joint);
  return joint;
}

void lovrDistanceJointGetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors((Joint*) joint, anchor1, anchor2);
}

void lovrDistanceJointSetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support modifying joint anchors after creation");
  // todo: no setter available, but the constraint could be removed and re-added
}

float lovrDistanceJointGetDistance(DistanceJoint* joint) {
  return JPH_DistanceConstraint_GetMaxDistance((JPH_DistanceConstraint*) joint->constraint);
}

void lovrDistanceJointSetDistance(DistanceJoint* joint, float distance) {
  JPH_DistanceConstraint_SetDistance((JPH_DistanceConstraint*) joint->constraint, distance, distance);
}

float lovrDistanceJointGetResponseTime(Joint* joint) {
  JPH_SpringSettings* settings = JPH_DistanceConstraint_GetLimitsSpringSettings((JPH_DistanceConstraint*) joint->constraint);
  return 1.f / JPH_SpringSettings_GetFrequency(settings);
}

void lovrDistanceJointSetResponseTime(Joint* joint, float responseTime) {
  JPH_SpringSettings* settings = JPH_SpringSettings_Create(1.f / responseTime, 0.f);
  JPH_DistanceConstraint_SetLimitsSpringSettings((JPH_DistanceConstraint*) joint->constraint, settings);
  JPH_SpringSettings_Destroy(settings);
}

float lovrDistanceJointGetTightness(Joint* joint) {
  // todo: jolt has spring damping instead of tightness, not compatible with lovr API
  // (but body's damping is not that different)
  lovrLog(LOG_WARN, "PHY", "Jolt does not support DistanceJoint tightness");
  return 0.f;
}

void lovrDistanceJointSetTightness(Joint* joint, float tightness) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support DistanceJoint tightness");
}

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]) {
  lovrCheck(a->world == b->world, "Joint bodies must exist in the same World");

  HingeJoint* joint = lovrCalloc(sizeof(HingeJoint));
  joint->ref = 1;
  joint->type = JOINT_HINGE;

  JPH_HingeConstraintSettings* settings = JPH_HingeConstraintSettings_Create();

  JPH_RVec3 point = { anchor[0], anchor[1], anchor[2] };
  JPH_Vec3 axisVec = { axis[0], axis[1], axis[2] };
  JPH_HingeConstraintSettings_SetPoint1(settings, &point);
  JPH_HingeConstraintSettings_SetPoint2(settings, &point);
  JPH_HingeConstraintSettings_SetHingeAxis1(settings, &axisVec);
  JPH_HingeConstraintSettings_SetHingeAxis2(settings, &axisVec);
  joint->constraint = (JPH_Constraint*) JPH_HingeConstraintSettings_CreateConstraint(settings, a->body, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(a->world->system, joint->constraint);
  arr_push(&a->joints, joint);
  arr_push(&b->joints, joint);
  lovrRetain(joint);
  return joint;
}

void lovrHingeJointGetAnchors(HingeJoint* joint, float anchor1[3], float anchor2[3]) {
  lovrJointGetAnchors(joint, anchor1, anchor2);
}

void lovrHingeJointSetAnchor(HingeJoint* joint, float anchor[3]) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support modifying joint anchors after creation");
  // todo: no setter available, but the constraint could be removed and re-added
}

void lovrHingeJointGetAxis(HingeJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_HingeConstraintSettings* settings = JPH_HingeConstraint_GetSettings((JPH_HingeConstraint*) joint->constraint);
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
  axis[0] = translation[0];
  axis[1] = translation[1];
  axis[2] = translation[2];
}

void lovrHingeJointSetAxis(HingeJoint* joint, float axis[3]) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support modifying joint axis after creation");
  // todo: no setter available, but the constraint could be removed and re-added
}


float lovrHingeJointGetAngle(HingeJoint* joint) {
  return -JPH_HingeConstraint_GetCurrentAngle((JPH_HingeConstraint*) joint->constraint);
}

float lovrHingeJointGetLowerLimit(HingeJoint* joint) {
  return JPH_HingeConstraint_GetLimitsMin((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointSetLowerLimit(HingeJoint* joint, float limit) {
  float upper_limit = JPH_HingeConstraint_GetLimitsMax((JPH_HingeConstraint*) joint->constraint);
  JPH_HingeConstraint_SetLimits((JPH_HingeConstraint*) joint->constraint, limit, upper_limit);
}

float lovrHingeJointGetUpperLimit(HingeJoint* joint) {
  return JPH_HingeConstraint_GetLimitsMax((JPH_HingeConstraint*) joint->constraint);
}

void lovrHingeJointSetUpperLimit(HingeJoint* joint, float limit) {
  float lower_limit = JPH_HingeConstraint_GetLimitsMin((JPH_HingeConstraint*) joint->constraint);
  JPH_HingeConstraint_SetLimits((JPH_HingeConstraint*) joint->constraint, lower_limit, limit);
}

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float axis[3]) {
  lovrCheck(a->world == b->world, "Joint bodies must exist in the same World");

  SliderJoint* joint = lovrCalloc(sizeof(SliderJoint));
  joint->ref = 1;
  joint->type = JOINT_SLIDER;

  JPH_SliderConstraintSettings* settings = JPH_SliderConstraintSettings_Create();
  const JPH_Vec3 axisVec = { axis[0], axis[1], axis[2] };
  JPH_SliderConstraintSettings_SetSliderAxis(settings, &axisVec);
  joint->constraint = (JPH_Constraint*) JPH_SliderConstraintSettings_CreateConstraint(settings, a->body, b->body);
  JPH_ConstraintSettings_Destroy((JPH_ConstraintSettings*) settings);
  JPH_PhysicsSystem_AddConstraint(a->world->system, joint->constraint);
  arr_push(&a->joints, joint);
  arr_push(&b->joints, joint);
  lovrRetain(joint);
  return joint;
}

void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]) {
  JPH_Vec3 resultAxis;
  JPH_SliderConstraintSettings* settings = JPH_SliderConstraint_GetSettings((JPH_SliderConstraint*) joint->constraint);
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

void lovrSliderJointSetAxis(SliderJoint* joint, float axis[3]) {
  lovrLog(LOG_WARN, "PHY", "Jolt does not support modifying joint axis after creation");
  // todo: no setter available, but the constraint could be removed and re-added
}

float lovrSliderJointGetPosition(SliderJoint* joint) {
  return JPH_SliderConstraint_GetCurrentPosition((JPH_SliderConstraint*) joint->constraint);
}

float lovrSliderJointGetLowerLimit(SliderJoint* joint) {
  return JPH_SliderConstraint_GetLimitsMin((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointSetLowerLimit(SliderJoint* joint, float limit) {
  float upper_limit = JPH_SliderConstraint_GetLimitsMax((JPH_SliderConstraint*) joint->constraint);
  JPH_SliderConstraint_SetLimits((JPH_SliderConstraint*) joint->constraint, limit, upper_limit);
}

float lovrSliderJointGetUpperLimit(SliderJoint* joint) {
  return JPH_SliderConstraint_GetLimitsMax((JPH_SliderConstraint*) joint->constraint);
}

void lovrSliderJointSetUpperLimit(SliderJoint* joint, float limit) {
  float lower_limit = JPH_SliderConstraint_GetLimitsMin((JPH_SliderConstraint*) joint->constraint);
  JPH_SliderConstraint_SetLimits((JPH_SliderConstraint*) joint->constraint, lower_limit, limit);
}
