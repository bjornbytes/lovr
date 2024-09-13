#include "api.h"
#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <stdbool.h>

static Collider* luax_checkcollider(lua_State* L, int index) {
  Collider* collider = luax_checktype(L, index, Collider);
  luax_check(L, !lovrColliderIsDestroyed(collider), "Attempt to use a destroyed Collider");
  return collider;
}

static int l_lovrColliderDestroy(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lovrColliderDestruct(collider);
  return 0;
}

static int l_lovrColliderIsDestroyed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool destroyed = lovrColliderIsDestroyed(collider);
  lua_pushboolean(L, destroyed);
  return 1;
}

static int l_lovrColliderIsEnabled(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool enabled = lovrColliderIsEnabled(collider);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrColliderSetEnabled(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool enable = lua_toboolean(L, 2);
  luax_assert(L, lovrColliderSetEnabled(collider, enable));
  return 1;
}

static int l_lovrColliderGetWorld(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  World* world = lovrColliderGetWorld(collider);
  luax_pushtype(L, World, world);
  return 1;
}

static int l_lovrColliderGetJoints(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_newtable(L);
  int index = 1;
  Joint* joint = NULL;
  while ((joint = lovrColliderGetJoints(collider, joint)) != NULL) {
    luax_pushjoint(L, joint);
    lua_rawseti(L, -2, index++);
  }
  return 1;
}

static int l_lovrColliderGetShapes(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_newtable(L);
  int index = 1;
  Shape* shape = NULL;
  while ((shape = lovrColliderGetShapes(collider, shape)) != NULL) {
    luax_pushshape(L, shape);
    lua_rawseti(L, -2, index++);
  }
  return 1;
}

static int l_lovrColliderGetShape(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  Shape* shape = lovrColliderGetShapes(collider, NULL);
  if (shape) {
    luax_pushshape(L, shape);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrColliderAddShape(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  Shape* shape = luax_checkshape(L, 2);
  luax_assert(L, lovrColliderAddShape(collider, shape));
  lua_settop(L, 2);
  return 1;
}

static int l_lovrColliderRemoveShape(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  Shape* shape = luax_checkshape(L, 2);
  luax_assert(L, lovrColliderRemoveShape(collider, shape));
  lua_settop(L, 2);
  return 1;
}

static int l_lovrColliderGetUserData(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushlightuserdata(L, collider);
  lua_rawget(L, LUA_REGISTRYINDEX);
  return 1;
}

static int l_lovrColliderSetUserData(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lovrColliderSetUserData(collider, (uintptr_t) lua_tothread(L, -1));
  lua_pushlightuserdata(L, collider);
  lua_pushvalue(L, 2);
  lua_rawset(L, LUA_REGISTRYINDEX);
  return 0;
}

static int l_lovrColliderIsKinematic(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushboolean(L, lovrColliderIsKinematic(collider));
  return 1;
}

static int l_lovrColliderSetKinematic(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool kinematic = lua_toboolean(L, 2);
  luax_assert(L, lovrColliderSetKinematic(collider, kinematic));
  return 0;
}

static int l_lovrColliderIsSensor(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushboolean(L, lovrColliderIsSensor(collider));
  return 1;
}

static int l_lovrColliderSetSensor(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool sensor = lua_toboolean(L, 2);
  lovrColliderSetSensor(collider, sensor);
  return 0;
}

static int l_lovrColliderIsContinuous(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool continuous = lovrColliderIsContinuous(collider);
  lua_pushboolean(L, continuous);
  return 1;
}

static int l_lovrColliderSetContinuous(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool continuous = lua_toboolean(L, 2);
  luax_assert(L, lovrColliderSetContinuous(collider, continuous));
  return 0;
}

static int l_lovrColliderGetGravityScale(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float scale = lovrColliderGetGravityScale(collider);
  lua_pushnumber(L, scale);
  return 1;
}

static int l_lovrColliderSetGravityScale(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float scale = luax_checkfloat(L, 2);
  luax_assert(L, lovrColliderSetGravityScale(collider, scale));
  return 0;
}

static int l_lovrColliderIsAwake(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushboolean(L, lovrColliderIsAwake(collider));
  return 1;
}

static int l_lovrColliderSetAwake(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool awake = lua_toboolean(L, 2);
  luax_assert(L, lovrColliderSetAwake(collider, awake));
  return 0;
}

static int l_lovrColliderIsSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushboolean(L, lovrColliderIsSleepingAllowed(collider));
  return 1;
}

static int l_lovrColliderSetSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool allowed = lua_toboolean(L, 2);
  lovrColliderSetSleepingAllowed(collider, allowed);
  return 0;
}

static int l_lovrColliderGetMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushnumber(L, lovrColliderGetMass(collider));
  return 1;
}

static int l_lovrColliderSetMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float mass = luax_checkfloat(L, 2);
  luax_assert(L, lovrColliderSetMass(collider, mass));
  return 0;
}

static int l_lovrColliderGetInertia(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float diagonal[3], rotation[4], angle, ax, ay, az;
  lovrColliderGetInertia(collider, diagonal, rotation);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, diagonal[0]);
  lua_pushnumber(L, diagonal[1]);
  lua_pushnumber(L, diagonal[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrColliderSetInertia(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float diagonal[3], rotation[4];
  int index = luax_readvec3(L, 2, diagonal, NULL);
  luax_readquat(L, index, rotation, NULL);
  lovrColliderSetInertia(collider, diagonal, rotation);
  return 0;
}

static int l_lovrColliderGetCenterOfMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float center[3];
  lovrColliderGetCenterOfMass(collider, center);
  lua_pushnumber(L, center[0]);
  lua_pushnumber(L, center[1]);
  lua_pushnumber(L, center[2]);
  return 3;
}

static int l_lovrColliderSetCenterOfMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float center[3];
  luax_readvec3(L, 2, center, NULL);
  luax_assert(L, lovrColliderSetCenterOfMass(collider, center));
  return 0;
}

static int l_lovrColliderGetAutomaticMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool enabled = lovrColliderGetAutomaticMass(collider);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrColliderSetAutomaticMass(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool enable = lua_toboolean(L, 2);
  lovrColliderSetAutomaticMass(collider, enable);
  return 0;
}

static int l_lovrColliderResetMassData(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  luax_assert(L, lovrColliderResetMassData(collider));
  return 0;
}

static int l_lovrColliderGetDegreesOfFreedom(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool translation[3];
  bool rotation[3];
  lovrColliderGetDegreesOfFreedom(collider, translation, rotation);

  char string[3];
  size_t length;

  length = 0;
  for (char i = 0; i < 3; i++) {
    if (translation[i]) {
      string[length++] = 'x' + i;
    }
  }
  lua_pushlstring(L, string, length);

  length = 0;
  for (char i = 0; i < 3; i++) {
    if (rotation[i]) {
      string[length++] = 'x' + i;
    }
  }
  lua_pushlstring(L, string, length);

  return 2;
}

static int l_lovrColliderSetDegreesOfFreedom(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool translation[3] = { false, false, false };
  bool rotation[3] = { false, false, false };
  const char* string;
  size_t length;

  string = lua_tolstring(L, 2, &length);
  for (size_t i = 0; i < length; i++) {
    if (string[i] >= 'x' && string[i] <= 'z') {
      translation[string[i] - 'x'] = true;
    }
  }

  string = lua_tolstring(L, 3, &length);
  for (size_t i = 0; i < length; i++) {
    if (string[i] >= 'x' && string[i] <= 'z') {
      rotation[string[i] - 'x'] = true;
    }
  }

  lovrColliderSetDegreesOfFreedom(collider, translation, rotation);
  return 0;
}

static int l_lovrColliderGetPosition(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float position[3];
  lovrColliderGetPosition(collider, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrColliderSetPosition(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float position[3];
  luax_readvec3(L, 2, position, NULL);
  luax_assert(L, lovrColliderSetPosition(collider, position));
  return 0;
}

static int l_lovrColliderGetOrientation(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float orientation[4], angle, x, y, z;
  lovrColliderGetOrientation(collider, orientation);
  quat_getAngleAxis(orientation, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

static int l_lovrColliderSetOrientation(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float orientation[4];
  luax_readquat(L, 2, orientation, NULL);
  luax_assert(L, lovrColliderSetOrientation(collider, orientation));
  return 0;
}

static int l_lovrColliderGetPose(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrColliderGetPose(collider, position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrColliderSetPose(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float position[3], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readquat(L, index, orientation, NULL);
  luax_assert(L, lovrColliderSetPose(collider, position, orientation));
  return 0;
}

static int l_lovrColliderMoveKinematic(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float position[3], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  float dt = luax_checkfloat(L, index);
  luax_assert(L, lovrColliderMoveKinematic(collider, position, orientation, dt));
  return 0;
}

static int l_lovrColliderGetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float velocity[3];
  lovrColliderGetLinearVelocity(collider, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderSetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float velocity[3];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetLinearVelocity(collider, velocity);
  return 0;
}

static int l_lovrColliderGetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float velocity[3];
  lovrColliderGetAngularVelocity(collider, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderSetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float velocity[3];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetAngularVelocity(collider, velocity);
  return 0;
}

static int l_lovrColliderGetLinearDamping(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float damping = lovrColliderGetLinearDamping(collider);
  lua_pushnumber(L, damping);
  return 1;
}

static int l_lovrColliderSetLinearDamping(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float damping = luax_checkfloat(L, 2);
  lovrColliderSetLinearDamping(collider, damping);
  return 0;
}

static int l_lovrColliderGetAngularDamping(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float damping = lovrColliderGetAngularDamping(collider);
  lua_pushnumber(L, damping);
  return 1;
}

static int l_lovrColliderSetAngularDamping(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float damping = luax_checkfloat(L, 2);
  lovrColliderSetAngularDamping(collider, damping);
  return 0;
}

static int l_lovrColliderApplyForce(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float force[3];
  int index = luax_readvec3(L, 2, force, NULL);

  if (lua_gettop(L) >= index) {
    float position[3];
    luax_readvec3(L, index, position, NULL);
    luax_assert(L, lovrColliderApplyForceAtPosition(collider, force, position));
  } else {
    luax_assert(L, lovrColliderApplyForce(collider, force));
  }

  return 0;
}

static int l_lovrColliderApplyTorque(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float torque[3];
  luax_readvec3(L, 2, torque, NULL);
  luax_assert(L, lovrColliderApplyTorque(collider, torque));
  return 0;
}

static int l_lovrColliderApplyLinearImpulse(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float impulse[3];
  int index = luax_readvec3(L, 2, impulse, NULL);
  if (lua_gettop(L) >= index) {
    float position[3];
    luax_readvec3(L, index, position, NULL);
    luax_assert(L, lovrColliderApplyLinearImpulseAtPosition(collider, impulse, position));
  } else {
    luax_assert(L, lovrColliderApplyLinearImpulse(collider, impulse));
  }
  return 0;
}

static int l_lovrColliderApplyAngularImpulse(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float impulse[3];
  luax_readvec3(L, 2, impulse, NULL);
  luax_assert(L, lovrColliderApplyAngularImpulse(collider, impulse));
  return 0;
}

static int l_lovrColliderGetLocalPoint(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float world[3], local[3];
  luax_readvec3(L, 2, world, NULL);
  lovrColliderGetLocalPoint(collider, world, local);
  lua_pushnumber(L, local[0]);
  lua_pushnumber(L, local[1]);
  lua_pushnumber(L, local[2]);
  return 3;
}

static int l_lovrColliderGetWorldPoint(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float local[3], world[3];
  luax_readvec3(L, 2, local, NULL);
  lovrColliderGetWorldPoint(collider, local, world);
  lua_pushnumber(L, world[0]);
  lua_pushnumber(L, world[1]);
  lua_pushnumber(L, world[2]);
  return 3;
}

static int l_lovrColliderGetLocalVector(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float world[3], local[3];
  luax_readvec3(L, 2, world, NULL);
  lovrColliderGetLocalVector(collider, world, local);
  lua_pushnumber(L, local[0]);
  lua_pushnumber(L, local[1]);
  lua_pushnumber(L, local[2]);
  return 3;
}

static int l_lovrColliderGetWorldVector(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float local[3], world[3];
  luax_readvec3(L, 2, local, NULL);
  lovrColliderGetWorldVector(collider, local, world);
  lua_pushnumber(L, world[0]);
  lua_pushnumber(L, world[1]);
  lua_pushnumber(L, world[2]);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromLocalPoint(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float point[3], velocity[3];
  luax_readvec3(L, 2, point, NULL);
  lovrColliderGetLinearVelocityFromLocalPoint(collider, point, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromWorldPoint(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float point[3], velocity[3];
  luax_readvec3(L, 2, point, NULL);
  lovrColliderGetLinearVelocityFromWorldPoint(collider, point, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderGetAABB(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float aabb[6];
  lovrColliderGetAABB(collider, aabb);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

static int l_lovrColliderGetFriction(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushnumber(L, lovrColliderGetFriction(collider));
  return 1;
}

static int l_lovrColliderSetFriction(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float friction = luax_checkfloat(L, 2);
  luax_assert(L, lovrColliderSetFriction(collider, friction));
  return 0;
}

static int l_lovrColliderGetRestitution(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushnumber(L, lovrColliderGetRestitution(collider));
  return 1;
}

static int l_lovrColliderSetRestitution(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  float restitution = luax_checkfloat(L, 2);
  luax_assert(L, lovrColliderSetRestitution(collider, restitution));
  return 0;
}

static int l_lovrColliderGetTag(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushstring(L, lovrColliderGetTag(collider));
  return 1;
}

static int l_lovrColliderSetTag(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  luax_assert(L, lovrColliderSetTag(collider, lua_tostring(L, 2)));
  return 0;
}

// Deprecated
static int l_lovrColliderIsGravityIgnored(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  lua_pushboolean(L, lovrColliderGetGravityScale(collider) == 0.f);
  return 1;
}

// Deprecated
static int l_lovrColliderSetGravityIgnored(lua_State* L) {
  Collider* collider = luax_checkcollider(L, 1);
  bool ignored = lua_toboolean(L, 2);
  lovrColliderSetGravityScale(collider, ignored ? 0.f : 1.f);
  return 0;
}

const luaL_Reg lovrCollider[] = {
  { "destroy", l_lovrColliderDestroy },
  { "isDestroyed", l_lovrColliderIsDestroyed },
  { "isEnabled", l_lovrColliderIsEnabled },
  { "setEnabled", l_lovrColliderSetEnabled },
  { "getWorld", l_lovrColliderGetWorld },
  { "getJoints", l_lovrColliderGetJoints },
  { "getShapes", l_lovrColliderGetShapes },
  { "getShape", l_lovrColliderGetShape },
  { "addShape", l_lovrColliderAddShape },
  { "removeShape", l_lovrColliderRemoveShape },
  { "getUserData", l_lovrColliderGetUserData },
  { "setUserData", l_lovrColliderSetUserData },
  { "isKinematic", l_lovrColliderIsKinematic },
  { "setKinematic", l_lovrColliderSetKinematic },
  { "isSensor", l_lovrColliderIsSensor },
  { "setSensor", l_lovrColliderSetSensor },
  { "isContinuous", l_lovrColliderIsContinuous },
  { "setContinuous", l_lovrColliderSetContinuous },
  { "getGravityScale", l_lovrColliderGetGravityScale },
  { "setGravityScale", l_lovrColliderSetGravityScale },
  { "isSleepingAllowed", l_lovrColliderIsSleepingAllowed },
  { "setSleepingAllowed", l_lovrColliderSetSleepingAllowed },
  { "isAwake", l_lovrColliderIsAwake },
  { "setAwake", l_lovrColliderSetAwake },
  { "getMass", l_lovrColliderGetMass },
  { "setMass", l_lovrColliderSetMass },
  { "getInertia", l_lovrColliderGetInertia },
  { "setInertia", l_lovrColliderSetInertia },
  { "getCenterOfMass", l_lovrColliderGetCenterOfMass },
  { "setCenterOfMass", l_lovrColliderSetCenterOfMass },
  { "getAutomaticMass", l_lovrColliderGetAutomaticMass },
  { "setAutomaticMass", l_lovrColliderSetAutomaticMass },
  { "resetMassData", l_lovrColliderResetMassData },
  { "getDegreesOfFreedom", l_lovrColliderGetDegreesOfFreedom },
  { "setDegreesOfFreedom", l_lovrColliderSetDegreesOfFreedom },
  { "getPosition", l_lovrColliderGetPosition },
  { "setPosition", l_lovrColliderSetPosition },
  { "getOrientation", l_lovrColliderGetOrientation },
  { "setOrientation", l_lovrColliderSetOrientation },
  { "getPose", l_lovrColliderGetPose },
  { "setPose", l_lovrColliderSetPose },
  { "moveKinematic", l_lovrColliderMoveKinematic },
  { "getLinearVelocity", l_lovrColliderGetLinearVelocity },
  { "setLinearVelocity", l_lovrColliderSetLinearVelocity },
  { "getAngularVelocity", l_lovrColliderGetAngularVelocity },
  { "setAngularVelocity", l_lovrColliderSetAngularVelocity },
  { "getLinearDamping", l_lovrColliderGetLinearDamping },
  { "setLinearDamping", l_lovrColliderSetLinearDamping },
  { "getAngularDamping", l_lovrColliderGetAngularDamping },
  { "setAngularDamping", l_lovrColliderSetAngularDamping },
  { "applyForce", l_lovrColliderApplyForce },
  { "applyTorque", l_lovrColliderApplyTorque },
  { "applyLinearImpulse", l_lovrColliderApplyLinearImpulse },
  { "applyAngularImpulse", l_lovrColliderApplyAngularImpulse },
  { "getLocalPoint", l_lovrColliderGetLocalPoint },
  { "getWorldPoint", l_lovrColliderGetWorldPoint },
  { "getLocalVector", l_lovrColliderGetLocalVector },
  { "getWorldVector", l_lovrColliderGetWorldVector },
  { "getLinearVelocityFromLocalPoint", l_lovrColliderGetLinearVelocityFromLocalPoint },
  { "getLinearVelocityFromWorldPoint", l_lovrColliderGetLinearVelocityFromWorldPoint },
  { "getAABB", l_lovrColliderGetAABB },
  { "getFriction", l_lovrColliderGetFriction },
  { "setFriction", l_lovrColliderSetFriction },
  { "getRestitution", l_lovrColliderGetRestitution },
  { "setRestitution", l_lovrColliderSetRestitution },
  { "getTag", l_lovrColliderGetTag },
  { "setTag", l_lovrColliderSetTag },

  // Deprecated
  { "isGravityIgnored", l_lovrColliderIsGravityIgnored },
  { "setGravityIgnored", l_lovrColliderSetGravityIgnored },

  { NULL, NULL }
};
