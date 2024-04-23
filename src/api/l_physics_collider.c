#include "api.h"
#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <stdbool.h>

static int l_lovrColliderDestroy(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lovrColliderDestroyData(collider);
  return 0;
}

static int l_lovrColliderIsDestroyed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool destroyed = lovrColliderIsDestroyed(collider);
  lua_pushboolean(L, destroyed);
  return 1;
}

static int l_lovrColliderIsEnabled(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool enabled = lovrColliderIsEnabled(collider);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrColliderSetEnabled(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool enable = lua_toboolean(L, 2);
  lovrColliderSetEnabled(collider, enable);
  return 1;
}

static int l_lovrColliderGetWorld(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  World* world = lovrColliderGetWorld(collider);
  luax_pushtype(L, World, world);
  return 1;
}

static int l_lovrColliderGetShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  uint32_t child = lua_gettop(L) == 1 ? ~0u : luax_checku32(L, 2) - 1;
  Shape* shape = lovrColliderGetShape(collider, child);
  if (shape) {
    luax_pushshape(L, shape);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrColliderSetShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = lua_isnoneornil(L, 2) ? NULL : luax_checkshape(L, 2);
  lovrColliderSetShape(collider, shape);
  return 0;
}

static int l_lovrColliderGetJoints(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_newtable(L);
  int index = 1;
  Joint* joint = NULL;
  while ((joint = lovrColliderGetJoints(collider, joint)) != NULL) {
    luax_pushjoint(L, joint);
    lua_rawseti(L, -2, index++);
  }
  return 1;
}

static int l_lovrColliderGetUserData(lua_State* L) {
  luax_checktype(L, 1, Collider);
  luax_pushstash(L, "lovr.collider.userdata");
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrColliderSetUserData(lua_State* L) {
  luax_checktype(L, 1, Collider);
  luax_pushstash(L, "lovr.collider.userdata");
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_rawset(L, -3);
  return 0;
}

static int l_lovrColliderIsKinematic(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsKinematic(collider));
  return 1;
}

static int l_lovrColliderSetKinematic(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool kinematic = lua_toboolean(L, 2);
  lovrColliderSetKinematic(collider, kinematic);
  return 0;
}

static int l_lovrColliderIsSensor(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsSensor(collider));
  return 1;
}

static int l_lovrColliderSetSensor(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool sensor = lua_toboolean(L, 2);
  lovrColliderSetSensor(collider, sensor);
  return 0;
}

static int l_lovrColliderIsContinuous(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool continuous = lovrColliderIsContinuous(collider);
  lua_pushboolean(L, continuous);
  return 1;
}

static int l_lovrColliderSetContinuous(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool continuous = lua_toboolean(L, 2);
  lovrColliderSetContinuous(collider, continuous);
  return 0;
}

static int l_lovrColliderGetGravityScale(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float scale = lovrColliderGetGravityScale(collider);
  lua_pushnumber(L, scale);
  return 1;
}

static int l_lovrColliderSetGravityScale(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float scale = luax_checkfloat(L, 2);
  lovrColliderSetGravityScale(collider, scale);
  return 0;
}

static int l_lovrColliderIsAwake(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsAwake(collider));
  return 1;
}

static int l_lovrColliderSetAwake(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool awake = lua_toboolean(L, 2);
  lovrColliderSetAwake(collider, awake);
  return 0;
}

static int l_lovrColliderIsSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsSleepingAllowed(collider));
  return 1;
}

static int l_lovrColliderSetSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool allowed = lua_toboolean(L, 2);
  lovrColliderSetSleepingAllowed(collider, allowed);
  return 0;
}

static int l_lovrColliderGetMass(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushnumber(L, lovrColliderGetMass(collider));
  return 1;
}

static int l_lovrColliderSetMass(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float mass = luax_checkfloat(L, 2);
  lovrColliderSetMass(collider, mass);
  return 0;
}

static int l_lovrColliderGetMassData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float centerOfMass[3], mass, inertia[6];
  lovrColliderGetMassData(collider, centerOfMass, &mass, inertia);
  lua_pushnumber(L, centerOfMass[0]);
  lua_pushnumber(L, centerOfMass[1]);
  lua_pushnumber(L, centerOfMass[2]);
  lua_pushnumber(L, mass);
  lua_newtable(L);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, inertia[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 5;
}

static int l_lovrColliderSetMassData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float centerOfMass[3];
  int index = luax_readvec3(L, 2, centerOfMass, NULL);
  float mass = luax_checkfloat(L, index++);
  float inertia[6];
  if (lua_istable(L, index) && luax_len(L, index) >= 6) {
    for (int i = 0; i < 6; i++) {
      lua_rawgeti(L, index, i + 1);
      if (!lua_isnumber(L, -1)) {
        return luaL_argerror(L, 6, "Expected 6 numbers or a table with 6 numbers");
      }

      inertia[i] = lua_tonumber(L, -1);
      lua_pop(L, 1);
    }
  } else {
    for (int i = index; i < index + 6; i++) {
      if (lua_isnumber(L, i)) {
        inertia[i - index] = lua_tonumber(L, i);
      } else {
        return luaL_argerror(L, i, "Expected 6 numbers or a table with 6 numbers");
      }
    }
  }
  lovrColliderSetMassData(collider, centerOfMass, mass, inertia);
  return 0;
}

static int l_lovrColliderGetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[3];
  lovrColliderGetPosition(collider, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrColliderSetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[3];
  luax_readvec3(L, 2, position, NULL);
  lovrColliderSetPosition(collider, position);
  return 0;
}

static int l_lovrColliderGetOrientation(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
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
  Collider* collider = luax_checktype(L, 1, Collider);
  float orientation[4];
  luax_readquat(L, 2, orientation, NULL);
  lovrColliderSetOrientation(collider, orientation);
  return 0;
}

static int l_lovrColliderGetPose(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrColliderGetPosition(collider, position);
  lovrColliderGetOrientation(collider, orientation);
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
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[3], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readquat(L, index, orientation, NULL);
  lovrColliderSetPosition(collider, position);
  lovrColliderSetOrientation(collider, orientation);
  return 0;
}

static int l_lovrColliderGetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[3];
  lovrColliderGetLinearVelocity(collider, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderSetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[3];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetLinearVelocity(collider, velocity);
  return 0;
}

static int l_lovrColliderGetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[3];
  lovrColliderGetAngularVelocity(collider, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderSetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[3];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetAngularVelocity(collider, velocity);
  return 0;
}

static int l_lovrColliderGetLinearDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping, threshold;
  lovrColliderGetLinearDamping(collider, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrColliderSetLinearDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrColliderSetLinearDamping(collider, damping, threshold);
  return 0;
}

static int l_lovrColliderGetAngularDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping, threshold;
  lovrColliderGetAngularDamping(collider, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrColliderSetAngularDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrColliderSetAngularDamping(collider, damping, threshold);
  return 0;
}

static int l_lovrColliderApplyForce(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float force[3];
  int index = luax_readvec3(L, 2, force, NULL);

  if (lua_gettop(L) >= index) {
    float position[3];
    luax_readvec3(L, index, position, NULL);
    lovrColliderApplyForceAtPosition(collider, force, position);
  } else {
    lovrColliderApplyForce(collider, force);
  }

  return 0;
}

static int l_lovrColliderApplyTorque(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float torque[3];
  luax_readvec3(L, 2, torque, NULL);
  lovrColliderApplyTorque(collider, torque);
  return 0;
}

static int l_lovrColliderApplyLinearImpulse(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float impulse[3];
  int index = luax_readvec3(L, 2, impulse, NULL);
  if (lua_gettop(L) >= index) {
    float position[3];
    luax_readvec3(L, index, position, NULL);
    lovrColliderApplyLinearImpulseAtPosition(collider, impulse, position);
  } else {
    lovrColliderApplyLinearImpulse(collider, impulse);
  }
  return 0;
}

static int l_lovrColliderApplyAngularImpulse(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float impulse[3];
  luax_readvec3(L, 2, impulse, NULL);
  lovrColliderApplyAngularImpulse(collider, impulse);
  return 0;
}

static int l_lovrColliderGetLocalCenter(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float center[3];
  lovrColliderGetLocalCenter(collider, center);
  lua_pushnumber(L, center[0]);
  lua_pushnumber(L, center[1]);
  lua_pushnumber(L, center[2]);
  return 3;
}

static int l_lovrColliderGetWorldCenter(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float center[3];
  lovrColliderGetWorldCenter(collider, center);
  lua_pushnumber(L, center[0]);
  lua_pushnumber(L, center[1]);
  lua_pushnumber(L, center[2]);
  return 3;
}

static int l_lovrColliderGetLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float world[3], local[3];
  luax_readvec3(L, 2, world, NULL);
  lovrColliderGetLocalPoint(collider, world, local);
  lua_pushnumber(L, local[0]);
  lua_pushnumber(L, local[1]);
  lua_pushnumber(L, local[2]);
  return 3;
}

static int l_lovrColliderGetWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float local[3], world[3];
  luax_readvec3(L, 2, local, NULL);
  lovrColliderGetWorldPoint(collider, local, world);
  lua_pushnumber(L, world[0]);
  lua_pushnumber(L, world[1]);
  lua_pushnumber(L, world[2]);
  return 3;
}

static int l_lovrColliderGetLocalVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float world[3], local[3];
  luax_readvec3(L, 2, world, NULL);
  lovrColliderGetLocalVector(collider, world, local);
  lua_pushnumber(L, local[0]);
  lua_pushnumber(L, local[1]);
  lua_pushnumber(L, local[2]);
  return 3;
}

static int l_lovrColliderGetWorldVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float local[3], world[3];
  luax_readvec3(L, 2, local, NULL);
  lovrColliderGetWorldVector(collider, local, world);
  lua_pushnumber(L, world[0]);
  lua_pushnumber(L, world[1]);
  lua_pushnumber(L, world[2]);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float point[3], velocity[3];
  luax_readvec3(L, 2, point, NULL);
  lovrColliderGetLinearVelocityFromLocalPoint(collider, point, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float point[3], velocity[3];
  luax_readvec3(L, 2, point, NULL);
  lovrColliderGetLinearVelocityFromWorldPoint(collider, point, velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrColliderGetAABB(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float aabb[6];
  lovrColliderGetAABB(collider, aabb);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

static int l_lovrColliderGetFriction(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushnumber(L, lovrColliderGetFriction(collider));
  return 1;
}

static int l_lovrColliderSetFriction(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float friction = luax_checkfloat(L, 2);
  lovrColliderSetFriction(collider, friction);
  return 0;
}

static int l_lovrColliderGetRestitution(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushnumber(L, lovrColliderGetRestitution(collider));
  return 1;
}

static int l_lovrColliderSetRestitution(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float restitution = luax_checkfloat(L, 2);
  lovrColliderSetRestitution(collider, restitution);
  return 0;
}

static int l_lovrColliderGetTag(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushstring(L, lovrColliderGetTag(collider));
  return 1;
}

static int l_lovrColliderSetTag(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  if (lua_isnoneornil(L, 2)) {
    lovrColliderSetTag(collider, NULL);
    return 0;
  }

  const char* tag = luaL_checkstring(L, 2);
  if (!lovrColliderSetTag(collider, tag)) {
    return luaL_error(L, "Invalid tag %s", tag);
  }

  return 0;
}

// Deprecated
static int l_lovrColliderGetShapes(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = lovrColliderGetShape(collider, ~0u);
  lua_createtable(L, 1, 0);
  luax_pushshape(L, shape);
  lua_rawseti(L, -2, 1);
  return 1;
}

// Deprecated
static int l_lovrColliderIsGravityIgnored(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderGetGravityScale(collider) == 0.f);
  return 1;
}

// Deprecated
static int l_lovrColliderSetGravityIgnored(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
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
  { "getShape", l_lovrColliderGetShape },
  { "setShape", l_lovrColliderSetShape },
  { "getJoints", l_lovrColliderGetJoints },
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
  { "getMassData", l_lovrColliderGetMassData },
  { "setMassData", l_lovrColliderSetMassData },
  { "getPosition", l_lovrColliderGetPosition },
  { "setPosition", l_lovrColliderSetPosition },
  { "getOrientation", l_lovrColliderGetOrientation },
  { "setOrientation", l_lovrColliderSetOrientation },
  { "getPose", l_lovrColliderGetPose },
  { "setPose", l_lovrColliderSetPose },
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
  { "getLocalCenter", l_lovrColliderGetLocalCenter },
  { "getWorldCenter", l_lovrColliderGetWorldCenter },
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
  { "getShapes", l_lovrColliderGetShapes },
  { "isGravityIgnored", l_lovrColliderIsGravityIgnored },
  { "setGravityIgnored", l_lovrColliderSetGravityIgnored },

  { NULL, NULL }
};
