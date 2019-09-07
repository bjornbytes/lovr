#include "core/arr.h"
#include <stdint.h>
#include <stdbool.h>
#include <ode/ode.h>

#pragma once

#define MAX_CONTACTS 4
#define MAX_TAGS 16
#define NO_TAG ~0u

typedef enum {
  SHAPE_SPHERE,
  SHAPE_BOX,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER
} ShapeType;

typedef enum {
  JOINT_BALL,
  JOINT_DISTANCE,
  JOINT_HINGE,
  JOINT_SLIDER
} JointType;

typedef struct Collider Collider;
typedef struct Shape Shape;
typedef struct Joint Joint;

typedef struct {
  dWorldID id;
  dSpaceID space;
  dJointGroupID contactGroup;
  arr_t(Shape*) overlaps;
  char* tags[MAX_TAGS];
  uint16_t masks[MAX_TAGS];
  Collider* head;
} World;

struct Collider {
  dBodyID body;
  World* world;
  Collider* prev;
  Collider* next;
  void* userdata;
  uint32_t tag;
  arr_t(Shape*) shapes;
  arr_t(Joint*) joints;
  float friction;
  float restitution;
};

struct Shape {
  ShapeType type;
  dGeomID id;
  Collider* collider;
  void* userdata;
};

typedef Shape SphereShape;
typedef Shape BoxShape;
typedef Shape CapsuleShape;
typedef Shape CylinderShape;

struct Joint {
  JointType type;
  dJointID id;
  void* userdata;
};

typedef Joint BallJoint;
typedef Joint DistanceJoint;
typedef Joint HingeJoint;
typedef Joint SliderJoint;

typedef void (*CollisionResolver)(World* world, void* userdata);
typedef void (*RaycastCallback)(Shape* shape, float x, float y, float z, float nx, float ny, float nz, void* userdata);

typedef struct {
  RaycastCallback callback;
  void* userdata;
} RaycastData;

bool lovrPhysicsInit(void);
void lovrPhysicsDestroy(void);

World* lovrWorldInit(World* world, float xg, float yg, float zg, bool allowSleep, const char** tags, uint32_t tagCount);
#define lovrWorldCreate(...) lovrWorldInit(lovrAlloc(World), __VA_ARGS__)
void lovrWorldDestroy(void* ref);
void lovrWorldDestroyData(World* world);
void lovrWorldUpdate(World* world, float dt, CollisionResolver resolver, void* userdata);
void lovrWorldComputeOverlaps(World* world);
int lovrWorldGetNextOverlap(World* world, Shape** a, Shape** b);
int lovrWorldCollide(World* world, Shape* a, Shape* b, float friction, float restitution);
void lovrWorldGetGravity(World* world, float* x, float* y, float* z);
void lovrWorldSetGravity(World* world, float x, float y, float z);
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold);
void lovrWorldSetLinearDamping(World* world, float damping, float threshold);
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold);
void lovrWorldSetAngularDamping(World* world, float damping, float threshold);
bool lovrWorldIsSleepingAllowed(World* world);
void lovrWorldSetSleepingAllowed(World* world, bool allowed);
void lovrWorldRaycast(World* world, float x1, float y1, float z1, float x2, float y2, float z2, RaycastCallback callback, void* userdata);
const char* lovrWorldGetTagName(World* world, uint32_t tag);
int lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2);
int lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2);
int lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag);

Collider* lovrColliderInit(Collider* collider, World* world, float x, float y, float z);
#define lovrColliderCreate(...) lovrColliderInit(lovrAlloc(Collider), __VA_ARGS__)
void lovrColliderDestroy(void* ref);
void lovrColliderDestroyData(Collider* collider);
World* lovrColliderGetWorld(Collider* collider);
void lovrColliderAddShape(Collider* collider, Shape* shape);
void lovrColliderRemoveShape(Collider* collider, Shape* shape);
Shape** lovrColliderGetShapes(Collider* collider, size_t* count);
Joint** lovrColliderGetJoints(Collider* collider, size_t* count);
void* lovrColliderGetUserData(Collider* collider);
void lovrColliderSetUserData(Collider* collider, void* data);
const char* lovrColliderGetTag(Collider* collider);
bool lovrColliderSetTag(Collider* collider, const char* tag);
float lovrColliderGetFriction(Collider* collider);
void lovrColliderSetFriction(Collider* collider, float friction);
float lovrColliderGetRestitution(Collider* collider);
void lovrColliderSetRestitution(Collider* collider, float restitution);
bool lovrColliderIsKinematic(Collider* collider);
void lovrColliderSetKinematic(Collider* collider, bool kinematic);
bool lovrColliderIsGravityIgnored(Collider* collider);
void lovrColliderSetGravityIgnored(Collider* collider, bool ignored);
bool lovrColliderIsSleepingAllowed(Collider* collider);
void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed);
bool lovrColliderIsAwake(Collider* collider);
void lovrColliderSetAwake(Collider* collider, bool awake);
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
void lovrColliderGetAABB(Collider* collider, float aabb[6]);

void lovrShapeDestroy(void* ref);
void lovrShapeDestroyData(Shape* shape);
ShapeType lovrShapeGetType(Shape* shape);
Collider* lovrShapeGetCollider(Shape* shape);
bool lovrShapeIsEnabled(Shape* shape);
void lovrShapeSetEnabled(Shape* shape, bool enabled);
void* lovrShapeGetUserData(Shape* shape);
void lovrShapeSetUserData(Shape* shape, void* data);
void lovrShapeGetPosition(Shape* shape, float* x, float* y, float* z);
void lovrShapeSetPosition(Shape* shape, float x, float y, float z);
void lovrShapeGetOrientation(Shape* shape, float* angle, float* x, float* y, float* z);
void lovrShapeSetOrientation(Shape* shape, float angle, float x, float y, float z);
void lovrShapeGetMass(Shape* shape, float density, float* cx, float* cy, float* cz, float* mass, float inertia[6]);
void lovrShapeGetAABB(Shape* shape, float aabb[6]);

SphereShape* lovrSphereShapeInit(SphereShape* sphere, float radius);
#define lovrSphereShapeCreate(...) lovrSphereShapeInit(lovrAlloc(SphereShape), __VA_ARGS__)
#define lovrSphereShapeDestroy lovrShapeDestroy
float lovrSphereShapeGetRadius(SphereShape* sphere);
void lovrSphereShapeSetRadius(SphereShape* sphere, float radius);

BoxShape* lovrBoxShapeInit(BoxShape* box, float x, float y, float z);
#define lovrBoxShapeCreate(...) lovrBoxShapeInit(lovrAlloc(BoxShape), __VA_ARGS__)
#define lovrBoxShapeDestroy lovrShapeDestroy
void lovrBoxShapeGetDimensions(BoxShape* box, float* x, float* y, float* z);
void lovrBoxShapeSetDimensions(BoxShape* box, float x, float y, float z);

CapsuleShape* lovrCapsuleShapeInit(CapsuleShape* capsule, float radius, float length);
#define lovrCapsuleShapeCreate(...) lovrCapsuleShapeInit(lovrAlloc(CapsuleShape), __VA_ARGS__)
#define lovrCapsuleShapeDestroy lovrShapeDestroy
float lovrCapsuleShapeGetRadius(CapsuleShape* capsule);
void lovrCapsuleShapeSetRadius(CapsuleShape* capsule, float radius);
float lovrCapsuleShapeGetLength(CapsuleShape* capsule);
void lovrCapsuleShapeSetLength(CapsuleShape* capsule, float length);

CylinderShape* lovrCylinderShapeInit(CylinderShape* cylinder, float radius, float length);
#define lovrCylinderShapeCreate(...) lovrCylinderShapeInit(lovrAlloc(CylinderShape), __VA_ARGS__)
#define lovrCylinderShapeDestroy lovrShapeDestroy
float lovrCylinderShapeGetRadius(CylinderShape* cylinder);
void lovrCylinderShapeSetRadius(CylinderShape* cylinder, float radius);
float lovrCylinderShapeGetLength(CylinderShape* cylinder);
void lovrCylinderShapeSetLength(CylinderShape* cylinder, float length);

void lovrJointDestroy(void* ref);
void lovrJointDestroyData(Joint* joint);
JointType lovrJointGetType(Joint* joint);
void lovrJointGetColliders(Joint* joint, Collider** a, Collider** b);
void* lovrJointGetUserData(Joint* joint);
void lovrJointSetUserData(Joint* joint, void* data);

BallJoint* lovrBallJointInit(BallJoint* joint, Collider* a, Collider* b, float x, float y, float z);
#define lovrBallJointCreate(...) lovrBallJointInit(lovrAlloc(BallJoint), __VA_ARGS__)
#define lovrBallJointDestroy lovrJointDestroy
void lovrBallJointGetAnchors(BallJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2);
void lovrBallJointSetAnchor(BallJoint* joint, float x, float y, float z);

DistanceJoint* lovrDistanceJointInit(DistanceJoint* joint, Collider* a, Collider* b, float x1, float y1, float z1, float x2, float y2, float z2);
#define lovrDistanceJointCreate(...) lovrDistanceJointInit(lovrAlloc(DistanceJoint), __VA_ARGS__)
#define lovrDistanceJointDestroy lovrJointDestroy
void lovrDistanceJointGetAnchors(DistanceJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2);
void lovrDistanceJointSetAnchors(DistanceJoint* joint, float x1, float y1, float z1, float x2, float y2, float z2);
float lovrDistanceJointGetDistance(DistanceJoint* joint);
void lovrDistanceJointSetDistance(DistanceJoint* joint, float distance);

HingeJoint* lovrHingeJointInit(HingeJoint* joint, Collider* a, Collider* b, float x, float y, float z, float ax, float ay, float az);
#define lovrHingeJointCreate(...) lovrHingeJointInit(lovrAlloc(HingeJoint), __VA_ARGS__)
#define lovrHingeJointDestroy lovrJointDestroy
void lovrHingeJointGetAnchors(HingeJoint* joint, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2);
void lovrHingeJointSetAnchor(HingeJoint* joint, float x, float y, float z);
void lovrHingeJointGetAxis(HingeJoint* joint, float* x, float* y, float* z);
void lovrHingeJointSetAxis(HingeJoint* joint, float x, float y, float z);
float lovrHingeJointGetAngle(HingeJoint* joint);
float lovrHingeJointGetLowerLimit(HingeJoint* joint);
void lovrHingeJointSetLowerLimit(HingeJoint* joint, float limit);
float lovrHingeJointGetUpperLimit(HingeJoint* joint);
void lovrHingeJointSetUpperLimit(HingeJoint* joint, float limit);

SliderJoint* lovrSliderJointInit(SliderJoint* joint, Collider* a, Collider* b, float ax, float ay, float az);
#define lovrSliderJointCreate(...) lovrSliderJointInit(lovrAlloc(SliderJoint), __VA_ARGS__)
#define lovrSliderJointDestroy lovrJointDestroy
void lovrSliderJointGetAxis(SliderJoint* joint, float* x, float* y, float* z);
void lovrSliderJointSetAxis(SliderJoint* joint, float x, float y, float z);
float lovrSliderJointGetPosition(SliderJoint* joint);
float lovrSliderJointGetLowerLimit(SliderJoint* joint);
void lovrSliderJointSetLowerLimit(SliderJoint* joint, float limit);
float lovrSliderJointGetUpperLimit(SliderJoint* joint);
void lovrSliderJointSetUpperLimit(SliderJoint* joint, float limit);
