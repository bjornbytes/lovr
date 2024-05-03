#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_CONTACTS 10
#define MAX_TAGS 31
#define NO_TAG ~0u

typedef struct World World;
typedef struct Collider Collider;
typedef struct Shape Shape;
typedef struct Joint Joint;

typedef Shape BoxShape;
typedef Shape SphereShape;
typedef Shape CapsuleShape;
typedef Shape CylinderShape;
typedef Shape ConvexShape;
typedef Shape MeshShape;
typedef Shape TerrainShape;
typedef Shape CompoundShape;

typedef Joint WeldJoint;
typedef Joint BallJoint;
typedef Joint ConeJoint;
typedef Joint DistanceJoint;
typedef Joint HingeJoint;
typedef Joint SliderJoint;

bool lovrPhysicsInit(void);
void lovrPhysicsDestroy(void);

// World

typedef struct {
  uint32_t tickRate;
  uint32_t tickLimit;
  uint32_t maxColliders;
  bool deterministic;
  bool threadSafe;
  bool allowSleep;
  float stabilization;
  float maxPenetration;
  float minBounceVelocity;
  uint32_t velocitySteps;
  uint32_t positionSteps;
  const char* tags[MAX_TAGS];
  uint32_t staticTagMask;
  uint32_t tagCount;
} WorldInfo;

typedef struct {
  Collider* collider;
  Shape* shape;
  float position[3];
  float normal[3];
  float fraction;
} CastResult;

typedef CastResult CollideResult;

typedef float CastCallback(void* userdata, CastResult* hit);
typedef float CollideCallback(void* userdata, CollideResult* hit);
typedef void QueryCallback(void* userdata, Collider* collider);

World* lovrWorldCreate(WorldInfo* info);
void lovrWorldDestroy(void* ref);
void lovrWorldDestroyData(World* world);
char** lovrWorldGetTags(World* world, uint32_t* count);
uint32_t lovrWorldGetTagMask(World* world, const char* string, size_t length);
uint32_t lovrWorldGetColliderCount(World* world);
uint32_t lovrWorldGetJointCount(World* world);
Collider* lovrWorldGetColliders(World* world, Collider* collider);
Joint* lovrWorldGetJoints(World* world, Joint* joint);
void lovrWorldGetGravity(World* world, float gravity[3]);
void lovrWorldSetGravity(World* world, float gravity[3]);
void lovrWorldUpdate(World* world, float dt);
bool lovrWorldRaycast(World* world, float start[3], float end[3], uint32_t filter, CastCallback* callback, void* userdata);
bool lovrWorldShapecast(World* world, Shape* shape, float pose[7], float scale, float end[3], uint32_t filter, CastCallback* callback, void* userdata);
bool lovrWorldCollideShape(World* world, Shape* shape, float pose[7], float scale, uint32_t filter, CollideCallback* callback, void* userdata);
bool lovrWorldQueryBox(World* world, float position[3], float size[3], uint32_t filter, QueryCallback* callback, void* userdata);
bool lovrWorldQuerySphere(World* world, float position[3], float radius, uint32_t filter, QueryCallback* callback, void* userdata);
void lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2);
void lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2);
bool lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2);

// Deprecated
int lovrWorldGetStepCount(World* world);
void lovrWorldSetStepCount(World* world, int iterations);
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

// Collider

Collider* lovrColliderCreate(World* world, float position[3], Shape* shape);
void lovrColliderDestroy(void* ref);
void lovrColliderDestroyData(Collider* collider);
bool lovrColliderIsDestroyed(Collider* collider);
bool lovrColliderIsEnabled(Collider* collider);
void lovrColliderSetEnabled(Collider* collider, bool enable);
World* lovrColliderGetWorld(Collider* collider);
Joint* lovrColliderGetJoints(Collider* collider, Joint* joint);
Shape* lovrColliderGetShape(Collider* collider);
void lovrColliderSetShape(Collider* collider, Shape* shape);
const char* lovrColliderGetTag(Collider* collider);
void lovrColliderSetTag(Collider* collider, const char* tag);
float lovrColliderGetFriction(Collider* collider);
void lovrColliderSetFriction(Collider* collider, float friction);
float lovrColliderGetRestitution(Collider* collider);
void lovrColliderSetRestitution(Collider* collider, float restitution);
bool lovrColliderIsKinematic(Collider* collider);
void lovrColliderSetKinematic(Collider* collider, bool kinematic);
bool lovrColliderIsSensor(Collider* collider);
void lovrColliderSetSensor(Collider* collider, bool sensor);
bool lovrColliderIsContinuous(Collider* collider);
void lovrColliderSetContinuous(Collider* collider, bool continuous);
float lovrColliderGetGravityScale(Collider* collider);
void lovrColliderSetGravityScale(Collider* collider, float scale);
bool lovrColliderIsSleepingAllowed(Collider* collider);
void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed);
bool lovrColliderIsAwake(Collider* collider);
void lovrColliderSetAwake(Collider* collider, bool awake);
float lovrColliderGetMass(Collider* collider);
void lovrColliderSetMass(Collider* collider, float* mass);
void lovrColliderGetInertia(Collider* collider, float diagonal[3], float rotation[4]);
void lovrColliderSetInertia(Collider* collider, float diagonal[3], float rotation[4]);
void lovrColliderGetCenterOfMass(Collider* collider, float center[3]);
void lovrColliderSetCenterOfMass(Collider* collider, float center[3]);
void lovrColliderResetMassData(Collider* collider);
void lovrColliderGetEnabledAxes(Collider* collider, bool translation[3], bool rotation[3]);
void lovrColliderSetEnabledAxes(Collider* collider, bool translation[3], bool rotation[3]);
void lovrColliderGetPosition(Collider* collider, float position[3]);
void lovrColliderSetPosition(Collider* collider, float position[3]);
void lovrColliderGetOrientation(Collider* collider, float orientation[4]);
void lovrColliderSetOrientation(Collider* collider, float orientation[4]);
void lovrColliderGetRawPosition(Collider* collider, float position[3]);
void lovrColliderGetRawOrientation(Collider* collider, float orientation[4]);
void lovrColliderGetLinearVelocity(Collider* collider, float velocity[3]);
void lovrColliderSetLinearVelocity(Collider* collider, float velocity[3]);
void lovrColliderGetAngularVelocity(Collider* collider, float velocity[3]);
void lovrColliderSetAngularVelocity(Collider* collider, float velocity[3]);
void lovrColliderGetLinearDamping(Collider* collider, float* damping, float* threshold);
void lovrColliderSetLinearDamping(Collider* collider, float damping, float threshold);
void lovrColliderGetAngularDamping(Collider* collider, float* damping, float* threshold);
void lovrColliderSetAngularDamping(Collider* collider, float damping, float threshold);
void lovrColliderApplyForce(Collider* collider, float force[3]);
void lovrColliderApplyForceAtPosition(Collider* collider, float force[3], float position[3]);
void lovrColliderApplyTorque(Collider* collider, float torque[3]);
void lovrColliderApplyLinearImpulse(Collider* collider, float impulse[3]);
void lovrColliderApplyLinearImpulseAtPosition(Collider* collider, float impulse[3], float position[3]);
void lovrColliderApplyAngularImpulse(Collider* collider, float impulse[3]);
void lovrColliderGetLocalPoint(Collider* collider, float world[3], float local[3]);
void lovrColliderGetWorldPoint(Collider* collider, float local[3], float world[3]);
void lovrColliderGetLocalVector(Collider* collider, float world[3], float local[3]);
void lovrColliderGetWorldVector(Collider* collider, float local[3], float world[3]);
void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float point[3], float velocity[3]);
void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float point[3], float velocity[3]);
void lovrColliderGetAABB(Collider* collider, float aabb[6]);

// Shapes

typedef enum {
  SHAPE_BOX,
  SHAPE_SPHERE,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER,
  SHAPE_CONVEX,
  SHAPE_MESH,
  SHAPE_TERRAIN,
  SHAPE_COMPOUND
} ShapeType;

void lovrShapeDestroy(void* ref);
ShapeType lovrShapeGetType(Shape* shape);
float lovrShapeGetVolume(Shape* shape);
float lovrShapeGetDensity(Shape* shape);
void lovrShapeSetDensity(Shape* shape, float density);
float lovrShapeGetMass(Shape* shape);
void lovrShapeGetInertia(Shape* shape, float diagonal[3], float rotation[4]);
void lovrShapeGetCenterOfMass(Shape* shape, float center[3]);
void lovrShapeGetAABB(Shape* shape, float position[3], float orientation[4], float aabb[6]);

BoxShape* lovrBoxShapeCreate(float dimensions[3]);
void lovrBoxShapeGetDimensions(BoxShape* shape, float dimensions[3]);

SphereShape* lovrSphereShapeCreate(float radius);
float lovrSphereShapeGetRadius(SphereShape* shape);

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length);
float lovrCapsuleShapeGetRadius(CapsuleShape* shape);
float lovrCapsuleShapeGetLength(CapsuleShape* shape);

CylinderShape* lovrCylinderShapeCreate(float radius, float length);
float lovrCylinderShapeGetRadius(CylinderShape* shape);
float lovrCylinderShapeGetLength(CylinderShape* shape);

ConvexShape* lovrConvexShapeCreate(float points[], uint32_t count);
uint32_t lovrConvexShapeGetPointCount(ConvexShape* shape);
void lovrConvexShapeGetPoint(ConvexShape* shape, uint32_t index, float point[3]);
uint32_t lovrConvexShapeGetFaceCount(ConvexShape* shape);
uint32_t lovrConvexShapeGetFace(ConvexShape* shape, uint32_t index, uint32_t* pointIndices, uint32_t capacity);

MeshShape* lovrMeshShapeCreate(int vertexCount, float vertices[], int indexCount, uint32_t indices[]);

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t n, float scaleXZ, float scaleY);

CompoundShape* lovrCompoundShapeCreate(Shape** shapes, float* positions, float* orientations, uint32_t count, bool freeze);
bool lovrCompoundShapeIsFrozen(CompoundShape* shape);
void lovrCompoundShapeAddChild(CompoundShape* shape, Shape* child, float position[3], float orientation[4]);
void lovrCompoundShapeReplaceChild(CompoundShape* shape, uint32_t index, Shape* child, float position[3], float orientation[4]);
void lovrCompoundShapeRemoveChild(CompoundShape* shape, uint32_t index);
Shape* lovrCompoundShapeGetChild(CompoundShape* shape, uint32_t index);
uint32_t lovrCompoundShapeGetChildCount(CompoundShape* shape);
void lovrCompoundShapeGetChildOffset(CompoundShape* shape, uint32_t index, float position[3], float orientation[4]);
void lovrCompoundShapeSetChildOffset(CompoundShape* shape, uint32_t index, float position[3], float orientation[4]);

// These tokens need to exist for Lua bindings
#define lovrBoxShapeDestroy lovrShapeDestroy
#define lovrSphereShapeDestroy lovrShapeDestroy
#define lovrCapsuleShapeDestroy lovrShapeDestroy
#define lovrCylinderShapeDestroy lovrShapeDestroy
#define lovrConvexShapeDestroy lovrShapeDestroy
#define lovrMeshShapeDestroy lovrShapeDestroy
#define lovrTerrainShapeDestroy lovrShapeDestroy
#define lovrCompoundShapeDestroy lovrShapeDestroy

// Joints

typedef enum {
  JOINT_WELD,
  JOINT_BALL,
  JOINT_CONE,
  JOINT_DISTANCE,
  JOINT_HINGE,
  JOINT_SLIDER
} JointType;

typedef enum {
  TARGET_NONE,
  TARGET_VELOCITY,
  TARGET_POSITION
} TargetType;

void lovrJointDestroy(void* ref);
void lovrJointDestroyData(Joint* joint);
bool lovrJointIsDestroyed(Joint* joint);
JointType lovrJointGetType(Joint* joint);
Collider* lovrJointGetColliderA(Joint* joint);
Collider* lovrJointGetColliderB(Joint* joint);
Joint* lovrJointGetNext(Joint* joint, Collider* collider);
uint32_t lovrJointGetPriority(Joint* joint);
void lovrJointSetPriority(Joint* joint, uint32_t priority);
bool lovrJointIsEnabled(Joint* joint);
void lovrJointSetEnabled(Joint* joint, bool enable);
float lovrJointGetForce(Joint* joint);
float lovrJointGetTorque(Joint* joint);

WeldJoint* lovrWeldJointCreate(Collider* a, Collider* b, float anchor[3]);
void lovrWeldJointGetAnchors(WeldJoint* joint, float anchor1[3], float anchor2[3]);

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]);
void lovrBallJointGetAnchors(BallJoint* joint, float anchor1[3], float anchor2[3]);

ConeJoint* lovrConeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]);
void lovrConeJointGetAnchors(ConeJoint* joint, float anchor1[3], float anchor2[3]);
void lovrConeJointGetAxis(ConeJoint* joint, float axis[3]);
float lovrConeJointGetLimit(ConeJoint* joint);
void lovrConeJointSetLimit(ConeJoint* joint, float angle);

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]);
void lovrDistanceJointGetAnchors(DistanceJoint* joint, float anchor1[3], float anchor2[3]);
void lovrDistanceJointGetLimits(DistanceJoint* joint, float* min, float* max);
void lovrDistanceJointSetLimits(DistanceJoint* joint, float min, float max);
void lovrDistanceJointGetSpring(DistanceJoint* joint, float* frequency, float* damping);
void lovrDistanceJointSetSpring(DistanceJoint* joint, float frequency, float damping);

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]);
void lovrHingeJointGetAnchors(HingeJoint* joint, float anchor1[3], float anchor2[3]);
void lovrHingeJointGetAxis(HingeJoint* joint, float axis[3]);
float lovrHingeJointGetAngle(HingeJoint* joint);
void lovrHingeJointGetLimits(HingeJoint* joint, float* min, float* max);
void lovrHingeJointSetLimits(HingeJoint* joint, float min, float max);
float lovrHingeJointGetFriction(HingeJoint* joint);
void lovrHingeJointSetFriction(HingeJoint* joint, float friction);
void lovrHingeJointGetMotorTarget(HingeJoint* joint, TargetType* type, float* value);
void lovrHingeJointSetMotorTarget(HingeJoint* joint, TargetType type, float value);
void lovrHingeJointGetMotorSpring(HingeJoint* joint, float* frequency, float* damping);
void lovrHingeJointSetMotorSpring(HingeJoint* joint, float frequency, float damping);
void lovrHingeJointGetMaxMotorForce(HingeJoint* joint, float* positive, float* negative);
void lovrHingeJointSetMaxMotorForce(HingeJoint* joint, float positive, float negative);
float lovrHingeJointGetMotorForce(HingeJoint* joint);
void lovrHingeJointGetSpring(HingeJoint* joint, float* frequency, float* damping);
void lovrHingeJointSetSpring(HingeJoint* joint, float frequency, float damping);

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float axis[3]);
void lovrSliderJointGetAnchors(SliderJoint* joint, float anchor1[3], float anchor2[3]);
void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]);
float lovrSliderJointGetPosition(SliderJoint* joint);
void lovrSliderJointGetLimits(SliderJoint* joint, float* min, float* max);
void lovrSliderJointSetLimits(SliderJoint* joint, float min, float max);
float lovrSliderJointGetFriction(SliderJoint* joint);
void lovrSliderJointSetFriction(SliderJoint* joint, float friction);
void lovrSliderJointGetMotorTarget(SliderJoint* joint, TargetType* type, float* value);
void lovrSliderJointSetMotorTarget(SliderJoint* joint, TargetType type, float value);
void lovrSliderJointGetMotorSpring(SliderJoint* joint, float* frequency, float* damping);
void lovrSliderJointSetMotorSpring(SliderJoint* joint, float frequency, float damping);
void lovrSliderJointGetMaxMotorForce(SliderJoint* joint, float* positive, float* negative);
void lovrSliderJointSetMaxMotorForce(SliderJoint* joint, float positive, float negative);
float lovrSliderJointGetMotorForce(SliderJoint* joint);
void lovrSliderJointGetSpring(SliderJoint* joint, float* frequency, float* damping);
void lovrSliderJointSetSpring(SliderJoint* joint, float frequency, float damping);

// These tokens need to exist for Lua bindings
#define lovrWeldJointDestroy lovrJointDestroy
#define lovrBallJointDestroy lovrJointDestroy
#define lovrConeJointDestroy lovrJointDestroy
#define lovrDistanceJointDestroy lovrJointDestroy
#define lovrHingeJointDestroy lovrJointDestroy
#define lovrSliderJointDestroy lovrJointDestroy
