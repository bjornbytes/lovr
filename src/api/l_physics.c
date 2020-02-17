#include "api.h"
#include "physics/physics.h"
#include "core/ref.h"

StringEntry ShapeTypes[] = {
  [SHAPE_SPHERE] = ENTRY("sphere"),
  [SHAPE_BOX] = ENTRY("box"),
  [SHAPE_CAPSULE] = ENTRY("capsule"),
  [SHAPE_CYLINDER] = ENTRY("cylinder"),
  { 0 }
};

StringEntry JointTypes[] = {
  [JOINT_BALL] = ENTRY("ball"),
  [JOINT_DISTANCE] = ENTRY("distance"),
  [JOINT_HINGE] = ENTRY("hinge"),
  [JOINT_SLIDER] = ENTRY("slider"),
  { 0 }
};

static int l_lovrPhysicsNewWorld(lua_State* L) {
  float xg = luax_optfloat(L, 1, 0.f);
  float yg = luax_optfloat(L, 2, -9.81f);
  float zg = luax_optfloat(L, 3, 0.f);
  bool allowSleep = lua_gettop(L) < 4 || lua_toboolean(L, 4);
  const char* tags[16];
  int tagCount;
  if (lua_type(L, 5) == LUA_TTABLE) {
    tagCount = luax_len(L, 5);
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
  luax_pushtype(L, World, world);
  lovrRelease(World, world);
  return 1;
}

static int l_lovrPhysicsNewBallJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x = luax_checkfloat(L, 3);
  float y = luax_checkfloat(L, 4);
  float z = luax_checkfloat(L, 5);
  BallJoint* joint = lovrBallJointCreate(a, b, x, y, z);
  luax_pushtype(L, BallJoint, joint);
  lovrRelease(Joint, joint);
  return 1;
}

static int l_lovrPhysicsNewBoxShape(lua_State* L) {
  float x = luax_optfloat(L, 1, 1.f);
  float y = luax_optfloat(L, 2, x);
  float z = luax_optfloat(L, 3, x);
  BoxShape* box = lovrBoxShapeCreate(x, y, z);
  luax_pushtype(L, BoxShape, box);
  lovrRelease(Shape, box);
  return 1;
}

static int l_lovrPhysicsNewCapsuleShape(lua_State* L) {
  float radius = luax_optfloat(L, 1, 1.f);
  float length = luax_optfloat(L, 2, 1.f);
  CapsuleShape* capsule = lovrCapsuleShapeCreate(radius, length);
  luax_pushtype(L, CapsuleShape, capsule);
  lovrRelease(Shape, capsule);
  return 1;
}

static int l_lovrPhysicsNewCylinderShape(lua_State* L) {
  float radius = luax_optfloat(L, 1, 1.f);
  float length = luax_optfloat(L, 2, 1.f);
  CylinderShape* cylinder = lovrCylinderShapeCreate(radius, length);
  luax_pushtype(L, CylinderShape, cylinder);
  lovrRelease(Shape, cylinder);
  return 1;
}

static int l_lovrPhysicsNewDistanceJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x1 = luax_checkfloat(L, 3);
  float y1 = luax_checkfloat(L, 4);
  float z1 = luax_checkfloat(L, 5);
  float x2 = luax_checkfloat(L, 6);
  float y2 = luax_checkfloat(L, 7);
  float z2 = luax_checkfloat(L, 8);
  DistanceJoint* joint = lovrDistanceJointCreate(a, b, x1, y1, z1, x2, y2, z2);
  luax_pushtype(L, DistanceJoint, joint);
  lovrRelease(Joint, joint);
  return 1;
}

static int l_lovrPhysicsNewHingeJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float x = luax_checkfloat(L, 3);
  float y = luax_checkfloat(L, 4);
  float z = luax_checkfloat(L, 5);
  float ax = luax_checkfloat(L, 6);
  float ay = luax_checkfloat(L, 7);
  float az = luax_checkfloat(L, 8);
  HingeJoint* joint = lovrHingeJointCreate(a, b, x, y, z, ax, ay, az);
  luax_pushtype(L, HingeJoint, joint);
  lovrRelease(Joint, joint);
  return 1;
}

static int l_lovrPhysicsNewSliderJoint(lua_State* L) {
  Collider* a = luax_checktype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float ax = luax_checkfloat(L, 3);
  float ay = luax_checkfloat(L, 4);
  float az = luax_checkfloat(L, 5);
  SliderJoint* joint = lovrSliderJointCreate(a, b, ax, ay, az);
  luax_pushtype(L, SliderJoint, joint);
  lovrRelease(Joint, joint);
  return 1;
}

static int l_lovrPhysicsNewSphereShape(lua_State* L) {
  float radius = luax_optfloat(L, 1, 1.f);
  SphereShape* sphere = lovrSphereShapeCreate(radius);
  luax_pushtype(L, SphereShape, sphere);
  lovrRelease(Shape, sphere);
  return 1;
}

static const luaL_Reg lovrPhysics[] = {
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

int luaopen_lovr_physics(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, World);
  luax_registertype(L, Collider);
  luax_registertype(L, BallJoint);
  luax_registertype(L, DistanceJoint);
  luax_registertype(L, HingeJoint);
  luax_registertype(L, SliderJoint);
  luax_registertype(L, SphereShape);
  luax_registertype(L, BoxShape);
  luax_registertype(L, CapsuleShape);
  luax_registertype(L, CylinderShape);
  if (lovrPhysicsInit()) {
    luax_atexit(L, lovrPhysicsDestroy);
  }
  return 1;
}
