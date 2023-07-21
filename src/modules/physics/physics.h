#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_CONTACTS 10
#define MAX_TAGS 16
#define NO_TAG ~0u

typedef struct World World;
typedef struct Collider Collider;
typedef struct Shape Shape;
typedef struct Joint Joint;

typedef Shape SphereShape;
typedef Shape BoxShape;
typedef Shape CapsuleShape;
typedef Shape CylinderShape;
typedef Shape MeshShape;
typedef Shape TerrainShape;

typedef Joint BallJoint;
typedef Joint DistanceJoint;
typedef Joint HingeJoint;
typedef Joint SliderJoint;

typedef void (*CollisionResolver)(World* world, void* userdata);
typedef bool (*RaycastCallback)(Shape* shape, float x, float y, float z, float nx, float ny, float nz, void* userdata);
typedef bool (*QueryCallback)(Shape* shape, void* userdata);

bool lovrPhysicsInit(void);
void lovrPhysicsDestroy(void);

// World

typedef struct {
  float x, y, z;
  float nx, ny, nz;
  float depth;
} Contact;

World* lovrWorldCreate(float xg, float yg, float zg, bool allowSleep, const char** tags, uint32_t tagCount);
void lovrWorldDestroy(void* ref);
void lovrWorldDestroyData(World* world);
void lovrWorldUpdate(World* world, float dt, CollisionResolver resolver, void* userdata);
int lovrWorldGetStepCount(World* world);
void lovrWorldSetStepCount(World* world, int iterations);
void lovrWorldComputeOverlaps(World* world);
int lovrWorldGetNextOverlap(World* world, Shape** a, Shape** b);
int lovrWorldCollide(World* world, Shape* a, Shape* b, float friction, float restitution);
void lovrWorldGetContacts(World* world, Shape* a, Shape* b, Contact contacts[MAX_CONTACTS], uint32_t* count);
void lovrWorldRaycast(World* world, float x1, float y1, float z1, float x2, float y2, float z2, RaycastCallback callback, void* userdata);
bool lovrWorldQueryBox(World* world, float position[3], float size[3], QueryCallback callback, void* userdata);
bool lovrWorldQuerySphere(World* world, float position[3], float radius, QueryCallback callback, void* userdata);
Collider* lovrWorldGetFirstCollider(World* world);
void lovrWorldGetGravity(World* world, float* x, float* y, float* z);
void lovrWorldSetGravity(World* world, float x, float y, float z);
float lovrWorldGetResponseTime(World* world);
void lovrWorldSetResponseTime(World* world, float responseTime);
float lovrWorldGetTightness(World* world);
void lovrWorldSetTightness(World* world, float tightness);
void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold);
void lovrWorldSetLinearDamping(World* world, float damping, float threshold);
void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold);
void lovrWorldSetAngularDamping(World* world, float damping, float threshold);
bool lovrWorldIsSleepingAllowed(World* world);
void lovrWorldSetSleepingAllowed(World* world, bool allowed);
const char* lovrWorldGetTagName(World* world, uint32_t tag);
int lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2);
int lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2);
int lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag);

// Collider

Collider* lovrColliderCreate(World* world, float x, float y, float z);
void lovrColliderDestroy(void* ref);
void lovrColliderDestroyData(Collider* collider);
bool lovrColliderIsDestroyed(Collider* collider);
void lovrColliderInitInertia(Collider* collider, Shape* shape);
World* lovrColliderGetWorld(Collider* collider);
Collider* lovrColliderGetNext(Collider* collider);
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
void lovrColliderGetOrientation(Collider* collider, float* orientation);
void lovrColliderSetOrientation(Collider* collider, float* orientation);
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

// Shapes

typedef enum {
  SHAPE_SPHERE,
  SHAPE_BOX,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER,
  SHAPE_MESH,
  SHAPE_TERRAIN
} ShapeType;

void lovrShapeDestroy(void* ref);
void lovrShapeDestroyData(Shape* shape);
ShapeType lovrShapeGetType(Shape* shape);
Collider* lovrShapeGetCollider(Shape* shape);
bool lovrShapeIsEnabled(Shape* shape);
void lovrShapeSetEnabled(Shape* shape, bool enabled);
bool lovrShapeIsSensor(Shape* shape);
void lovrShapeSetSensor(Shape* shape, bool sensor);
void* lovrShapeGetUserData(Shape* shape);
void lovrShapeSetUserData(Shape* shape, void* data);
void lovrShapeGetPosition(Shape* shape, float* x, float* y, float* z);
void lovrShapeSetPosition(Shape* shape, float x, float y, float z);
void lovrShapeGetOrientation(Shape* shape, float* orientation);
void lovrShapeSetOrientation(Shape* shape, float* orientation);
void lovrShapeGetMass(Shape* shape, float density, float* cx, float* cy, float* cz, float* mass, float inertia[6]);
void lovrShapeGetAABB(Shape* shape, float aabb[6]);

SphereShape* lovrSphereShapeCreate(float radius);
float lovrSphereShapeGetRadius(SphereShape* sphere);
void lovrSphereShapeSetRadius(SphereShape* sphere, float radius);

BoxShape* lovrBoxShapeCreate(float w, float h, float d);
void lovrBoxShapeGetDimensions(BoxShape* box, float* w, float* h, float* d);
void lovrBoxShapeSetDimensions(BoxShape* box, float w, float h, float d);

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

MeshShape* lovrMeshShapeCreate(int vertexCount, float vertices[], int indexCount, uint32_t indices[]);

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t widthSamples, uint32_t depthSamples, float horizontalScale, float verticalScale);

// These tokens need to exist for Lua bindings
#define lovrSphereShapeDestroy lovrShapeDestroy
#define lovrBoxShapeDestroy lovrShapeDestroy
#define lovrCapsuleShapeDestroy lovrShapeDestroy
#define lovrCylinderShapeDestroy lovrShapeDestroy
#define lovrMeshShapeDestroy lovrShapeDestroy
#define lovrTerrainShapeDestroy lovrShapeDestroy

// Joints

typedef enum {
  JOINT_BALL,
  JOINT_DISTANCE,
  JOINT_HINGE,
  JOINT_SLIDER
} JointType;

void lovrJointDestroy(void* ref);
void lovrJointDestroyData(Joint* joint);
JointType lovrJointGetType(Joint* joint);
float lovrJointGetCFM(Joint* joint);
void lovrJointSetCFM(Joint* joint, float cfm);
float lovrJointGetERP(Joint* joint);
void lovrJointSetERP(Joint* joint, float erp);
void lovrJointGetColliders(Joint* joint, Collider** a, Collider** b);
void* lovrJointGetUserData(Joint* joint);
void lovrJointSetUserData(Joint* joint, void* data);
bool lovrJointIsEnabled(Joint* joint);
void lovrJointSetEnabled(Joint* joint, bool enable);

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]);
void lovrBallJointGetAnchors(BallJoint* joint, float anchor1[3], float anchor2[3]);
void lovrBallJointSetAnchor(BallJoint* joint, float anchor[3]);
float lovrBallJointGetResponseTime(Joint* joint);
void lovrBallJointSetResponseTime(Joint* joint, float responseTime);
float lovrBallJointGetTightness(Joint* joint);
void lovrBallJointSetTightness(Joint* joint, float tightness);

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]);
void lovrDistanceJointGetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]);
void lovrDistanceJointSetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]);
float lovrDistanceJointGetDistance(DistanceJoint* joint);
void lovrDistanceJointSetDistance(DistanceJoint* joint, float distance);
float lovrDistanceJointGetResponseTime(Joint* joint);
void lovrDistanceJointSetResponseTime(Joint* joint, float responseTime);
float lovrDistanceJointGetTightness(Joint* joint);
void lovrDistanceJointSetTightness(Joint* joint, float tightness);

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]);
void lovrHingeJointGetAnchors(HingeJoint* joint, float anchor1[3], float anchor2[3]);
void lovrHingeJointSetAnchor(HingeJoint* joint, float anchor[3]);
void lovrHingeJointGetAxis(HingeJoint* joint, float axis[3]);
void lovrHingeJointSetAxis(HingeJoint* joint, float axis[3]);
float lovrHingeJointGetAngle(HingeJoint* joint);
float lovrHingeJointGetLowerLimit(HingeJoint* joint);
void lovrHingeJointSetLowerLimit(HingeJoint* joint, float limit);
float lovrHingeJointGetUpperLimit(HingeJoint* joint);
void lovrHingeJointSetUpperLimit(HingeJoint* joint, float limit);

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float axis[3]);
void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]);
void lovrSliderJointSetAxis(SliderJoint* joint, float axis[3]);
float lovrSliderJointGetPosition(SliderJoint* joint);
float lovrSliderJointGetLowerLimit(SliderJoint* joint);
void lovrSliderJointSetLowerLimit(SliderJoint* joint, float limit);
float lovrSliderJointGetUpperLimit(SliderJoint* joint);
void lovrSliderJointSetUpperLimit(SliderJoint* joint, float limit);

// These tokens need to exist for Lua bindings
#define lovrBallJointDestroy lovrJointDestroy
#define lovrDistanceJointDestroy lovrJointDestroy
#define lovrHingeJointDestroy lovrJointDestroy
#define lovrSliderJointDestroy lovrJointDestroy
