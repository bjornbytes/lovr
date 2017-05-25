#include "api/lovr.h"
#include "physics/physics.h"

map_int_t ShapeTypes;
map_int_t JointTypes;

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, "World", lovrWorld);
  luax_registertype(L, "Collider", lovrCollider);
  luax_extendtype(L, "Joint", "BallJoint", lovrJoint, lovrBallJoint);
  luax_extendtype(L, "Joint", "HingeJoint", lovrJoint, lovrHingeJoint);
  luax_extendtype(L, "Joint", "SliderJoint", lovrJoint, lovrSliderJoint);
  luax_extendtype(L, "Shape", "SphereShape", lovrShape, lovrSphereShape);
  luax_extendtype(L, "Shape", "BoxShape", lovrShape, lovrBoxShape);
  luax_extendtype(L, "Shape", "CapsuleShape", lovrShape, lovrCapsuleShape);
  luax_extendtype(L, "Shape", "CylinderShape", lovrShape, lovrCylinderShape);

  map_init(&JointTypes);
  map_set(&JointTypes, "ball", JOINT_BALL);
  map_set(&JointTypes, "hinge", JOINT_HINGE);
  map_set(&JointTypes, "slider", JOINT_SLIDER);

  map_init(&ShapeTypes);
  map_set(&ShapeTypes, "sphere", SHAPE_SPHERE);
  map_set(&ShapeTypes, "box", SHAPE_BOX);
  map_set(&ShapeTypes, "capsule", SHAPE_CAPSULE);
  map_set(&ShapeTypes, "cylinder", SHAPE_CYLINDER);

  lovrPhysicsInit();
  return 1;
}

int l_lovrPhysicsNewWorld(lua_State* L) {
  World* world = lovrWorldCreate();
  luax_pushtype(L, World, world);
  return 1;
}

int l_lovrPhysicsNewBallJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  Joint* joint = lovrBallJointCreate(a, b, x, y, z);
  luax_pushtype(L, BallJoint, joint);
  return 1;
}

int l_lovrPhysicsNewBoxShape(lua_State* L) {
  float x = luaL_optnumber(L, 1, 1.f);
  float y = luaL_optnumber(L, 2, x);
  float z = luaL_optnumber(L, 3, x);
  BoxShape* box = lovrBoxShapeCreate(x, y, z);
  luax_pushtype(L, BoxShape, box);
  return 1;
}

int l_lovrPhysicsNewCapsuleShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  CapsuleShape* capsule = lovrCapsuleShapeCreate(radius, length);
  luax_pushtype(L, CapsuleShape, capsule);
  return 1;
}

int l_lovrPhysicsNewCylinderShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  CylinderShape* cylinder = lovrCylinderShapeCreate(radius, length);
  luax_pushtype(L, CylinderShape, cylinder);
  return 1;
}

int l_lovrPhysicsNewHingeJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  float ax = luaL_checknumber(L, 6);
  float ay = luaL_checknumber(L, 7);
  float az = luaL_checknumber(L, 8);
  Joint* joint = lovrHingeJointCreate(a, b, x, y, z, ax, ay, az);
  luax_pushtype(L, HingeJoint, joint);
  return 1;
}

int l_lovrPhysicsNewSliderJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float ax = luaL_checknumber(L, 3);
  float ay = luaL_checknumber(L, 4);
  float az = luaL_checknumber(L, 5);
  Joint* joint = lovrSliderJointCreate(a, b, ax, ay, az);
  luax_pushtype(L, SliderJoint, joint);
  return 1;
}

int l_lovrPhysicsNewSphereShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  SphereShape* sphere = lovrSphereShapeCreate(radius);
  luax_pushtype(L, SphereShape, sphere);
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { "newBallJoint", l_lovrPhysicsNewBallJoint },
  { "newBoxShape", l_lovrPhysicsNewBoxShape },
  { "newCapsuleShape", l_lovrPhysicsNewCapsuleShape },
  { "newCylinderShape", l_lovrPhysicsNewCylinderShape },
  { "newHingeJoint", l_lovrPhysicsNewHingeJoint },
  { "newSliderJoint", l_lovrPhysicsNewSliderJoint },
  { "newSphereShape", l_lovrPhysicsNewSphereShape },
  { NULL, NULL }
};
