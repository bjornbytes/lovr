#include "api.h"
#include "physics/physics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>

static int l_lovrColliderDestroy(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lovrColliderDestroyData(collider);
  return 0;
}

static int l_lovrColliderGetWorld(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  World* world = lovrColliderGetWorld(collider);
  luax_pushtype(L, World, world);
  return 1;
}

static int l_lovrColliderAddShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = luax_checkshape(L, 2);
  lovrColliderAddShape(collider, shape);
  return 0;
}

static int l_lovrColliderRemoveShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = luax_checkshape(L, 2);
  lovrColliderRemoveShape(collider, shape);
  return 0;
}

static int l_lovrColliderGetShapes(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  size_t count;
  Shape** shapes = lovrColliderGetShapes(collider, &count);
  lua_createtable(L, (int) count, 0);
  for (size_t i = 0; i < count; i++) {
    luax_pushshape(L, shapes[i]);
    lua_rawseti(L, -2, (int) i + 1);
  }
  return 1;
}

static int l_lovrColliderGetJoints(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  size_t count;
  Joint** joints = lovrColliderGetJoints(collider, &count);
  lua_createtable(L, (int) count, 0);
  for (size_t i = 0; i < count; i++) {
    luax_pushjoint(L, joints[i]);
    lua_rawseti(L, -2, (int) i + 1);
  }
  return 1;
}

static int l_lovrColliderGetUserData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  union { int i; void* p; } ref = { .p = lovrColliderGetUserData(collider) };
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref.i);
  return 1;
}

static int l_lovrColliderSetUserData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  union { int i; void* p; } ref = { .p = lovrColliderGetUserData(collider) };
  if (ref.i) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref.i);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref.i = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrColliderSetUserData(collider, ref.p);
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

static int l_lovrColliderIsGravityIgnored(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsGravityIgnored(collider));
  return 1;
}

static int l_lovrColliderSetGravityIgnored(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  bool ignored = lua_toboolean(L, 2);
  lovrColliderSetGravityIgnored(collider, ignored);
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
  float cx, cy, cz, mass;
  float inertia[6];
  lovrColliderGetMassData(collider, &cx, &cy, &cz, &mass, inertia);
  lua_pushnumber(L, cx);
  lua_pushnumber(L, cy);
  lua_pushnumber(L, cz);
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
  float cx = luax_checkfloat(L, 2);
  float cy = luax_checkfloat(L, 3);
  float cz = luax_checkfloat(L, 4);
  float mass = luax_checkfloat(L, 5);
  float inertia[6];
  if (lua_istable(L, 6) && luax_len(L, 6) >= 6) {
    for (int i = 0; i < 6; i++) {
      lua_rawgeti(L, 6, i + 1);
      if (!lua_isnumber(L, -1)) {
        return luaL_argerror(L, 6, "Expected 6 numbers or a table with 6 numbers");
      }

      inertia[i] = lua_tonumber(L, -1);
      lua_pop(L, 1);
    }
  } else {
    for (int i = 6; i < 12; i++) {
      if (lua_isnumber(L, i)) {
        inertia[i - 6] = lua_tonumber(L, i);
      } else {
        return luaL_argerror(L, i, "Expected 6 numbers or a table with 6 numbers");
      }
    }
  }
  lovrColliderSetMassData(collider, cx, cy, cz, mass, inertia);
  return 0;
}

static int l_lovrColliderGetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetPosition(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderSetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[4];
  luax_readvec3(L, 2, position, NULL);
  lovrColliderSetPosition(collider, position[0], position[1], position[2]);
  return 0;
}

static int l_lovrColliderGetOrientation(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float angle, x, y, z, orientation[4];
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
  float x, y, z, angle, ax, ay, az, orientation[4];
  lovrColliderGetPosition(collider, &x, &y, &z);
  lovrColliderGetOrientation(collider, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrColliderSetPose(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float position[4], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readquat(L, index, orientation, NULL);
  lovrColliderSetPosition(collider, position[0], position[1], position[2]);
  lovrColliderSetOrientation(collider, orientation);
  return 0;
}

static int l_lovrColliderGetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetLinearVelocity(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderSetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[4];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetLinearVelocity(collider, velocity[0], velocity[1], velocity[2]);
  return 0;
}

static int l_lovrColliderGetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetAngularVelocity(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderSetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float velocity[4];
  luax_readvec3(L, 2, velocity, NULL);
  lovrColliderSetAngularVelocity(collider, velocity[0], velocity[1], velocity[2]);
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
  float force[4];
  int index = luax_readvec3(L, 2, force, NULL);

  if (lua_gettop(L) >= index) {
    float position[4];
    luax_readvec3(L, index, position, NULL);
    lovrColliderApplyForceAtPosition(collider, force[0], force[1], force[2],
      position[0], position[1], position[2]);
  } else {
    lovrColliderApplyForce(collider, force[0], force[1], force[2]);
  }

  return 0;
}

static int l_lovrColliderApplyTorque(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float force[4];
  luax_readvec3(L, 2, force, NULL);
  lovrColliderApplyTorque(collider, force[0], force[1], force[2]);
  return 0;
}

static int l_lovrColliderGetLocalCenter(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetLocalCenter(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderGetLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float world[4];
  luax_readvec3(L, 2, world, NULL);
  float x, y, z;
  lovrColliderGetLocalPoint(collider, world[0], world[1], world[2], &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderGetWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float local[4];
  luax_readvec3(L, 2, local, NULL);
  float wx, wy, wz;
  lovrColliderGetWorldPoint(collider, local[0], local[1], local[2], &wx, &wy, &wz);
  lua_pushnumber(L, wx);
  lua_pushnumber(L, wy);
  lua_pushnumber(L, wz);
  return 3;
}

static int l_lovrColliderGetLocalVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float world[4];
  luax_readvec3(L, 2, world, NULL);
  float x, y, z;
  lovrColliderGetLocalVector(collider, world[0], world[1], world[2], &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrColliderGetWorldVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float local[4];
  luax_readvec3(L, 2, local, NULL);
  float wx, wy, wz;
  lovrColliderGetWorldVector(collider, local[0], local[1], local[2], &wx, &wy, &wz);
  lua_pushnumber(L, wx);
  lua_pushnumber(L, wy);
  lua_pushnumber(L, wz);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float local[4];
  luax_readvec3(L, 2, local, NULL);
  float vx, vy, vz;
  lovrColliderGetLinearVelocityFromLocalPoint(collider, local[0], local[1], local[2], &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
  return 3;
}

static int l_lovrColliderGetLinearVelocityFromWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float world[4];
  luax_readvec3(L, 2, world, NULL);
  float vx, vy, vz;
  lovrColliderGetLinearVelocityFromWorldPoint(collider, world[0], world[1], world[2], &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
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

const luaL_Reg lovrCollider[] = {
  { "destroy", l_lovrColliderDestroy },
  { "getWorld", l_lovrColliderGetWorld },
  { "addShape", l_lovrColliderAddShape },
  { "removeShape", l_lovrColliderRemoveShape },
  { "getShapes", l_lovrColliderGetShapes },
  { "getJoints", l_lovrColliderGetJoints },
  { "getUserData", l_lovrColliderGetUserData },
  { "setUserData", l_lovrColliderSetUserData },
  { "isKinematic", l_lovrColliderIsKinematic },
  { "setKinematic", l_lovrColliderSetKinematic },
  { "isGravityIgnored", l_lovrColliderIsGravityIgnored },
  { "setGravityIgnored", l_lovrColliderSetGravityIgnored },
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
  { "getLocalCenter", l_lovrColliderGetLocalCenter },
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
  { NULL, NULL }
};
