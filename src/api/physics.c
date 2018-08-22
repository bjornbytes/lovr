#include "api.h"
#include "physics/physics.h"

const char* ShapeTypes[] = {
  [SHAPE_SPHERE] = "sphere",
  [SHAPE_BOX] = "box",
  [SHAPE_CAPSULE] = "capsule",
  [SHAPE_CYLINDER] = "cylinder",
  NULL
};

const char* JointTypes[] = {
  [JOINT_BALL] = "ball",
  [JOINT_DISTANCE] = "distance",
  [JOINT_HINGE] = "hinge",
  [JOINT_SLIDER] = "slider",
  NULL
};

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, "World", lovrWorld);
  luax_registertype(L, "Collider", lovrCollider);
  luax_extendtype(L, "Joint", "BallJoint", lovrJoint, lovrBallJoint);
  luax_extendtype(L, "Joint", "DistanceJoint", lovrJoint, lovrDistanceJoint);
  luax_extendtype(L, "Joint", "HingeJoint", lovrJoint, lovrHingeJoint);
  luax_extendtype(L, "Joint", "SliderJoint", lovrJoint, lovrSliderJoint);
  luax_extendtype(L, "Shape", "SphereShape", lovrShape, lovrSphereShape);
  luax_extendtype(L, "Shape", "BoxShape", lovrShape, lovrBoxShape);
  luax_extendtype(L, "Shape", "CapsuleShape", lovrShape, lovrCapsuleShape);
  luax_extendtype(L, "Shape", "CylinderShape", lovrShape, lovrCylinderShape);
  lovrPhysicsInit();
  return 1;
}

int l_lovrPhysicsNewWorld(lua_State* L) {
  float xg = luaL_optnumber(L, 1, 0.f);
  float yg = luaL_optnumber(L, 2, -9.81);
  float zg = luaL_optnumber(L, 3, 0.f);
  bool allowSleep = lua_gettop(L) < 4 || lua_toboolean(L, 4);
  const char* tags[16];
  int tagCount;
  if (lua_type(L, 5) == LUA_TTABLE) {
    tagCount = lua_objlen(L, 5);
    for (int i = 0; i < tagCount; i++) {
      lua_rawgeti(L, -1, i + 1);
      if (lua_isstring(L, -1)) {
        tags[i] = lua_tostring(L, -1);
      } else {
        return luaL_error(L, "World tags must be a table of strings");
      }
      lua_pop(L, 1);
    }
  } else {
    tagCount = 0;
  }
  World* world = lovrWorldCreate(xg, yg, zg, allowSleep, tags, tagCount);
  luax_pushobject(L, world);
  lovrRelease(world);
  return 1;
}

int l_lovrPhysicsNewBallJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  BallJoint* joint = lovrBallJointCreate(a, b, x, y, z);
  luax_pushobject(L, joint);
  lovrRelease(joint);
  return 1;
}

int l_lovrPhysicsNewBoxShape(lua_State* L) {
  float x = luaL_optnumber(L, 1, 1.f);
  float y = luaL_optnumber(L, 2, x);
  float z = luaL_optnumber(L, 3, x);
  BoxShape* box = lovrBoxShapeCreate(x, y, z);
  luax_pushobject(L, box);
  lovrRelease(box);
  return 1;
}

int l_lovrPhysicsNewCapsuleShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  CapsuleShape* capsule = lovrCapsuleShapeCreate(radius, length);
  luax_pushobject(L, capsule);
  lovrRelease(capsule);
  return 1;
}

int l_lovrPhysicsNewCylinderShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  CylinderShape* cylinder = lovrCylinderShapeCreate(radius, length);
  luax_pushobject(L, cylinder);
  lovrRelease(cylinder);
  return 1;
}

int l_lovrPhysicsNewDistanceJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x1 = luaL_checknumber(L, 3);
  float y1 = luaL_checknumber(L, 4);
  float z1 = luaL_checknumber(L, 5);
  float x2 = luaL_checknumber(L, 6);
  float y2 = luaL_checknumber(L, 7);
  float z2 = luaL_checknumber(L, 8);
  DistanceJoint* joint = lovrDistanceJointCreate(a, b, x1, y1, z1, x2, y2, z2);
  luax_pushobject(L, joint);
  lovrRelease(joint);
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
  HingeJoint* joint = lovrHingeJointCreate(a, b, x, y, z, ax, ay, az);
  luax_pushobject(L, joint);
  lovrRelease(joint);
  return 1;
}

int l_lovrPhysicsNewSliderJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float ax = luaL_checknumber(L, 3);
  float ay = luaL_checknumber(L, 4);
  float az = luaL_checknumber(L, 5);
  SliderJoint* joint = lovrSliderJointCreate(a, b, ax, ay, az);
  luax_pushobject(L, joint);
  lovrRelease(joint);
  return 1;
}

int l_lovrPhysicsNewSphereShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  SphereShape* sphere = lovrSphereShapeCreate(radius);
  luax_pushobject(L, sphere);
  lovrRelease(sphere);
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { "newBallJoint", l_lovrPhysicsNewBallJoint },
  { "newBoxShape", l_lovrPhysicsNewBoxShape },
  { "newCapsuleShape", l_lovrPhysicsNewCapsuleShape },
  { "newCylinderShape", l_lovrPhysicsNewCylinderShape },
  { "newDistanceJoint", l_lovrPhysicsNewDistanceJoint },
  { "newHingeJoint", l_lovrPhysicsNewHingeJoint },
  { "newSliderJoint", l_lovrPhysicsNewSliderJoint },
  { "newSphereShape", l_lovrPhysicsNewSphereShape },
  { NULL, NULL }
};
