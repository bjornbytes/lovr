#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_TAGS 31

typedef struct World World;
typedef struct Collider Collider;
typedef struct Contact Contact;
typedef struct Shape Shape;
typedef struct Joint Joint;

typedef Shape BoxShape;
typedef Shape SphereShape;
typedef Shape CapsuleShape;
typedef Shape CylinderShape;
typedef Shape ConvexShape;
typedef Shape MeshShape;
typedef Shape TerrainShape;

typedef Joint WeldJoint;
typedef Joint BallJoint;
typedef Joint ConeJoint;
typedef Joint DistanceJoint;
typedef Joint HingeJoint;
typedef Joint SliderJoint;

bool lovrPhysicsInit(void (*freeUserdata)(void* object, uintptr_t userdata));
void lovrPhysicsDestroy(void);

// World

typedef struct {
  bool (*filter)(void* userdata, World* world, Collider* a, Collider* b);
  void (*enter)(void* userdata, World* world, Collider* a, Collider* b, Contact* contact);
  void (*exit)(void* userdata, World* world, Collider* a, Collider* b);
  void (*contact)(void* userdata, World* world, Collider* a, Collider* b, Contact* contact);
  void* userdata;
} WorldCallbacks;

typedef struct {
  uint32_t maxColliders;
  bool threadSafe;
  bool allowSleep;
  float stabilization;
  float maxOverlap;
  float restitutionThreshold;
  uint32_t velocitySteps;
  uint32_t positionSteps;
  const char* tags[MAX_TAGS];
  uint32_t staticTagMask;
  uint32_t tagCount;
} WorldInfo;

typedef struct {
  Collider* collider;
  Shape* shape;
  uint32_t triangle;
  float position[3];
  float normal[3];
  float fraction;
} CastResult;

typedef CastResult OverlapResult;

typedef float CastCallback(void* userdata, CastResult* hit);
typedef float OverlapCallback(void* userdata, OverlapResult* hit);
typedef void QueryCallback(void* userdata, Collider* collider);

World* lovrWorldCreate(WorldInfo* info);
void lovrWorldDestroy(void* ref);
void lovrWorldDestruct(World* world);
bool lovrWorldIsDestroyed(World* world);
char** lovrWorldGetTags(World* world, uint32_t* count);
uint32_t lovrWorldGetTagMask(World* world, const char* string, size_t length);
uint32_t lovrWorldGetColliderCount(World* world);
uint32_t lovrWorldGetJointCount(World* world);
Collider* lovrWorldGetColliders(World* world, Collider* collider);
Joint* lovrWorldGetJoints(World* world, Joint* joint);
void lovrWorldGetGravity(World* world, float gravity[3]);
void lovrWorldSetGravity(World* world, float gravity[3]);
void lovrWorldUpdate(World* world, float dt);
void lovrWorldInterpolate(World* world, float alpha);
bool lovrWorldRaycast(World* world, float start[3], float end[3], uint32_t filter, CastCallback* callback, void* userdata);
bool lovrWorldShapecast(World* world, Shape* shape, float pose[7], float end[3], uint32_t filter, CastCallback* callback, void* userdata);
bool lovrWorldOverlapShape(World* world, Shape* shape, float pose[7], float maxDistance, uint32_t filter, OverlapCallback* callback, void* userdata);
bool lovrWorldQueryBox(World* world, float position[3], float size[3], uint32_t filter, QueryCallback* callback, void* userdata);
bool lovrWorldQuerySphere(World* world, float position[3], float radius, uint32_t filter, QueryCallback* callback, void* userdata);
bool lovrWorldDisableCollisionBetween(World* world, const char* tag1, const char* tag2);
bool lovrWorldEnableCollisionBetween(World* world, const char* tag1, const char* tag2);
bool lovrWorldIsCollisionEnabledBetween(World* world, const char* tag1, const char* tag2, bool* enabled);
void lovrWorldSetCallbacks(World* world, WorldCallbacks* callbacks);

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
void lovrColliderDestruct(Collider* collider);
bool lovrColliderIsDestroyed(Collider* collider);
uintptr_t lovrColliderGetUserData(Collider* collider);
void lovrColliderSetUserData(Collider* collider, uintptr_t userdata);
bool lovrColliderIsEnabled(Collider* collider);
bool lovrColliderSetEnabled(Collider* collider, bool enable);
World* lovrColliderGetWorld(Collider* collider);
Joint* lovrColliderGetJoints(Collider* collider, Joint* joint);
Shape* lovrColliderGetShapes(Collider* collider, Shape* shape);
bool lovrColliderAddShape(Collider* collider, Shape* shape);
bool lovrColliderRemoveShape(Collider* collider, Shape* shape);
const char* lovrColliderGetTag(Collider* collider);
bool lovrColliderSetTag(Collider* collider, const char* tag);
float lovrColliderGetFriction(Collider* collider);
bool lovrColliderSetFriction(Collider* collider, float friction);
float lovrColliderGetRestitution(Collider* collider);
bool lovrColliderSetRestitution(Collider* collider, float restitution);
bool lovrColliderIsKinematic(Collider* collider);
bool lovrColliderSetKinematic(Collider* collider, bool kinematic);
bool lovrColliderIsSensor(Collider* collider);
void lovrColliderSetSensor(Collider* collider, bool sensor);
bool lovrColliderIsContinuous(Collider* collider);
bool lovrColliderSetContinuous(Collider* collider, bool continuous);
float lovrColliderGetGravityScale(Collider* collider);
bool lovrColliderSetGravityScale(Collider* collider, float scale);
bool lovrColliderIsSleepingAllowed(Collider* collider);
void lovrColliderSetSleepingAllowed(Collider* collider, bool allowed);
bool lovrColliderIsAwake(Collider* collider);
bool lovrColliderSetAwake(Collider* collider, bool awake);
float lovrColliderGetMass(Collider* collider);
bool lovrColliderSetMass(Collider* collider, float mass);
void lovrColliderGetInertia(Collider* collider, float diagonal[3], float rotation[4]);
void lovrColliderSetInertia(Collider* collider, float diagonal[3], float rotation[4]);
void lovrColliderGetCenterOfMass(Collider* collider, float center[3]);
bool lovrColliderSetCenterOfMass(Collider* collider, float center[3]);
bool lovrColliderGetAutomaticMass(Collider* collider);
void lovrColliderSetAutomaticMass(Collider* collider, bool enable);
bool lovrColliderResetMassData(Collider* collider);
void lovrColliderGetDegreesOfFreedom(Collider* collider, bool translation[3], bool rotation[3]);
void lovrColliderSetDegreesOfFreedom(Collider* collider, bool translation[3], bool rotation[3]);
void lovrColliderGetPosition(Collider* collider, float position[3]);
bool lovrColliderSetPosition(Collider* collider, float position[3]);
void lovrColliderGetRawPosition(Collider* collider, float position[3]);
void lovrColliderGetOrientation(Collider* collider, float orientation[4]);
bool lovrColliderSetOrientation(Collider* collider, float orientation[4]);
void lovrColliderGetPose(Collider* collider, float position[3], float orientation[4]);
bool lovrColliderSetPose(Collider* collider, float position[3], float orientation[4]);
bool lovrColliderMoveKinematic(Collider* collider, float position[3], float orientation[4], float dt);
void lovrColliderGetLinearVelocity(Collider* collider, float velocity[3]);
bool lovrColliderSetLinearVelocity(Collider* collider, float velocity[3]);
void lovrColliderGetAngularVelocity(Collider* collider, float velocity[3]);
bool lovrColliderSetAngularVelocity(Collider* collider, float velocity[3]);
float lovrColliderGetLinearDamping(Collider* collider);
void lovrColliderSetLinearDamping(Collider* collider, float damping);
float lovrColliderGetAngularDamping(Collider* collider);
void lovrColliderSetAngularDamping(Collider* collider, float damping);
bool lovrColliderApplyForce(Collider* collider, float force[3]);
bool lovrColliderApplyForceAtPosition(Collider* collider, float force[3], float position[3]);
bool lovrColliderApplyTorque(Collider* collider, float torque[3]);
bool lovrColliderApplyLinearImpulse(Collider* collider, float impulse[3]);
bool lovrColliderApplyLinearImpulseAtPosition(Collider* collider, float impulse[3], float position[3]);
bool lovrColliderApplyAngularImpulse(Collider* collider, float impulse[3]);
void lovrColliderGetLocalPoint(Collider* collider, float world[3], float local[3]);
void lovrColliderGetWorldPoint(Collider* collider, float local[3], float world[3]);
void lovrColliderGetLocalVector(Collider* collider, float world[3], float local[3]);
void lovrColliderGetWorldVector(Collider* collider, float local[3], float world[3]);
void lovrColliderGetLinearVelocityFromLocalPoint(Collider* collider, float point[3], float velocity[3]);
void lovrColliderGetLinearVelocityFromWorldPoint(Collider* collider, float point[3], float velocity[3]);
void lovrColliderGetAABB(Collider* collider, float aabb[6]);

// Contact

void lovrContactDestroy(void* ref);
bool lovrContactIsValid(Contact* contact);
Collider* lovrContactGetColliderA(Contact* contact);
Collider* lovrContactGetColliderB(Contact* contact);
Shape* lovrContactGetShapeA(Contact* contact);
Shape* lovrContactGetShapeB(Contact* contact);
void lovrContactGetNormal(Contact* contact, float normal[3]);
float lovrContactGetOverlap(Contact* contact);
uint32_t lovrContactGetPointCount(Contact* contact);
void lovrContactGetPoint(Contact* contact, uint32_t index, float point[3]);
float lovrContactGetFriction(Contact* contact);
void lovrContactSetFriction(Contact* contact, float friction);
float lovrContactGetRestitution(Contact* contact);
void lovrContactSetRestitution(Contact* contact, float restitution);
bool lovrContactIsEnabled(Contact* contact);
void lovrContactSetEnabled(Contact* contact, bool enable);
void lovrContactGetSurfaceVelocity(Contact* contact, float velocity[3]);
void lovrContactSetSurfaceVelocity(Contact* contact, float velocity[3]);

// Shapes

typedef enum {
  SHAPE_BOX,
  SHAPE_SPHERE,
  SHAPE_CAPSULE,
  SHAPE_CYLINDER,
  SHAPE_CONVEX,
  SHAPE_MESH,
  SHAPE_TERRAIN
} ShapeType;

void lovrShapeDestroy(void* ref);
void lovrShapeDestruct(Shape* shape);
bool lovrShapeIsDestroyed(Shape* shape);
ShapeType lovrShapeGetType(Shape* shape);
Collider* lovrShapeGetCollider(Shape* shape);
uintptr_t lovrShapeGetUserData(Shape* shape);
void lovrShapeSetUserData(Shape* shape, uintptr_t userdata);
float lovrShapeGetVolume(Shape* shape);
float lovrShapeGetDensity(Shape* shape);
void lovrShapeSetDensity(Shape* shape, float density);
float lovrShapeGetMass(Shape* shape);
void lovrShapeGetInertia(Shape* shape, float diagonal[3], float rotation[4]);
void lovrShapeGetCenterOfMass(Shape* shape, float center[3]);
void lovrShapeGetOffset(Shape* shape, float position[3], float orientation[4]);
bool lovrShapeSetOffset(Shape* shape, float position[3], float orientation[4]);
void lovrShapeGetPose(Shape* shape, float position[3], float orientation[4]);
void lovrShapeGetAABB(Shape* shape, float aabb[6]);
bool lovrShapeContainsPoint(Shape* shape, float point[3]);
bool lovrShapeRaycast(Shape* shape, float start[3], float end[3], CastResult* hit);

BoxShape* lovrBoxShapeCreate(float dimensions[3]);
void lovrBoxShapeGetDimensions(BoxShape* shape, float dimensions[3]);
bool lovrBoxShapeSetDimensions(BoxShape* shape, float dimensions[3]);

SphereShape* lovrSphereShapeCreate(float radius);
float lovrSphereShapeGetRadius(SphereShape* shape);
bool lovrSphereShapeSetRadius(SphereShape* shape, float radius);

CapsuleShape* lovrCapsuleShapeCreate(float radius, float length);
float lovrCapsuleShapeGetRadius(CapsuleShape* shape);
bool lovrCapsuleShapeSetRadius(CapsuleShape* shape, float radius);
float lovrCapsuleShapeGetLength(CapsuleShape* shape);
bool lovrCapsuleShapeSetLength(CapsuleShape* shape, float length);

CylinderShape* lovrCylinderShapeCreate(float radius, float length);
float lovrCylinderShapeGetRadius(CylinderShape* shape);
bool lovrCylinderShapeSetRadius(CylinderShape* shape, float radius);
float lovrCylinderShapeGetLength(CylinderShape* shape);
bool lovrCylinderShapeSetLength(CylinderShape* shape, float length);

ConvexShape* lovrConvexShapeCreate(float points[], uint32_t count, float scale);
ConvexShape* lovrConvexShapeClone(ConvexShape* parent, float scale);
uint32_t lovrConvexShapeGetPointCount(ConvexShape* shape);
bool lovrConvexShapeGetPoint(ConvexShape* shape, uint32_t index, float point[3]);
uint32_t lovrConvexShapeGetFaceCount(ConvexShape* shape);
uint32_t lovrConvexShapeGetFace(ConvexShape* shape, uint32_t index, uint32_t* pointIndices, uint32_t capacity);
float lovrConvexShapeGetScale(ConvexShape* shape);

MeshShape* lovrMeshShapeCreate(uint32_t vertexCount, float vertices[], uint32_t indexCount, uint32_t indices[], float scale);
MeshShape* lovrMeshShapeClone(MeshShape* parent, float scale);
float lovrMeshShapeGetScale(MeshShape* shape);

TerrainShape* lovrTerrainShapeCreate(float* vertices, uint32_t n, float scaleXZ, float scaleY);

// These tokens need to exist for Lua bindings
#define lovrBoxShapeDestroy lovrShapeDestroy
#define lovrSphereShapeDestroy lovrShapeDestroy
#define lovrCapsuleShapeDestroy lovrShapeDestroy
#define lovrCylinderShapeDestroy lovrShapeDestroy
#define lovrConvexShapeDestroy lovrShapeDestroy
#define lovrMeshShapeDestroy lovrShapeDestroy
#define lovrTerrainShapeDestroy lovrShapeDestroy

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
  MOTOR_OFF,
  MOTOR_VELOCITY,
  MOTOR_POSITION
} MotorMode;

void lovrJointDestroy(void* ref);
void lovrJointDestruct(Joint* joint);
bool lovrJointIsDestroyed(Joint* joint);
JointType lovrJointGetType(Joint* joint);
Collider* lovrJointGetColliderA(Joint* joint);
Collider* lovrJointGetColliderB(Joint* joint);
Joint* lovrJointGetNext(Joint* joint, Collider* collider);
uintptr_t lovrJointGetUserData(Joint* joint);
void lovrJointSetUserData(Joint* joint, uintptr_t userdata);
void lovrJointGetAnchors(Joint* joint, float anchor1[3], float anchor2[3]);
uint32_t lovrJointGetPriority(Joint* joint);
void lovrJointSetPriority(Joint* joint, uint32_t priority);
bool lovrJointIsEnabled(Joint* joint);
void lovrJointSetEnabled(Joint* joint, bool enable);
float lovrJointGetForce(Joint* joint);
float lovrJointGetTorque(Joint* joint);

WeldJoint* lovrWeldJointCreate(Collider* a, Collider* b);

BallJoint* lovrBallJointCreate(Collider* a, Collider* b, float anchor[3]);

ConeJoint* lovrConeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]);
void lovrConeJointGetAxis(ConeJoint* joint, float axis[3]);
float lovrConeJointGetLimit(ConeJoint* joint);
bool lovrConeJointSetLimit(ConeJoint* joint, float angle);

DistanceJoint* lovrDistanceJointCreate(Collider* a, Collider* b, float anchor1[3], float anchor2[3]);
void lovrDistanceJointGetLimits(DistanceJoint* joint, float* min, float* max);
bool lovrDistanceJointSetLimits(DistanceJoint* joint, float min, float max);
void lovrDistanceJointGetSpring(DistanceJoint* joint, float* frequency, float* damping);
void lovrDistanceJointSetSpring(DistanceJoint* joint, float frequency, float damping);

HingeJoint* lovrHingeJointCreate(Collider* a, Collider* b, float anchor[3], float axis[3]);
void lovrHingeJointGetAxis(HingeJoint* joint, float axis[3]);
float lovrHingeJointGetAngle(HingeJoint* joint);
void lovrHingeJointGetLimits(HingeJoint* joint, float* min, float* max);
bool lovrHingeJointSetLimits(HingeJoint* joint, float min, float max);
float lovrHingeJointGetFriction(HingeJoint* joint);
void lovrHingeJointSetFriction(HingeJoint* joint, float friction);
MotorMode lovrHingeJointGetMotorMode(HingeJoint* joint);
void lovrHingeJointSetMotorMode(HingeJoint* joint, MotorMode mode);
float lovrHingeJointGetMotorTarget(HingeJoint* joint);
void lovrHingeJointSetMotorTarget(HingeJoint* joint, float target);
void lovrHingeJointGetMotorSpring(HingeJoint* joint, float* frequency, float* damping);
void lovrHingeJointSetMotorSpring(HingeJoint* joint, float frequency, float damping);
void lovrHingeJointGetMaxMotorTorque(HingeJoint* joint, float* positive, float* negative);
void lovrHingeJointSetMaxMotorTorque(HingeJoint* joint, float positive, float negative);
float lovrHingeJointGetMotorTorque(HingeJoint* joint);
void lovrHingeJointGetSpring(HingeJoint* joint, float* frequency, float* damping);
void lovrHingeJointSetSpring(HingeJoint* joint, float frequency, float damping);

SliderJoint* lovrSliderJointCreate(Collider* a, Collider* b, float axis[3]);
void lovrSliderJointGetAxis(SliderJoint* joint, float axis[3]);
float lovrSliderJointGetPosition(SliderJoint* joint);
void lovrSliderJointGetLimits(SliderJoint* joint, float* min, float* max);
bool lovrSliderJointSetLimits(SliderJoint* joint, float min, float max);
float lovrSliderJointGetFriction(SliderJoint* joint);
void lovrSliderJointSetFriction(SliderJoint* joint, float friction);
MotorMode lovrSliderJointGetMotorMode(SliderJoint* joint);
void lovrSliderJointSetMotorMode(SliderJoint* joint, MotorMode mode);
float lovrSliderJointGetMotorTarget(SliderJoint* joint);
void lovrSliderJointSetMotorTarget(SliderJoint* joint, float target);
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
