#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrColliderAddShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = luax_checktypeof(L, 2, Shape);
  lovrColliderAddShape(collider, shape);
  return 0;
}

int l_lovrColliderRemoveShape(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  Shape* shape = luax_checktypeof(L, 2, Shape);
  lovrColliderRemoveShape(collider, shape);
  return 0;
}

int l_lovrColliderGetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetPosition(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrColliderSetPosition(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrColliderSetPosition(collider, x, y, z);
  return 0;
}

int l_lovrColliderGetOrientation(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float angle, x, y, z;
  lovrColliderGetOrientation(collider, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrColliderSetOrientation(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float angle = luaL_checknumber(L, 2);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  lovrColliderSetOrientation(collider, angle, x, y, z);
  return 0;
}

int l_lovrColliderGetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetLinearVelocity(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrColliderSetLinearVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrColliderSetLinearVelocity(collider, x, y, z);
  return 0;
}

int l_lovrColliderGetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x, y, z;
  lovrColliderGetAngularVelocity(collider, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrColliderSetAngularVelocity(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrColliderSetAngularVelocity(collider, x, y, z);
  return 0;
}

int l_lovrColliderGetLinearDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping, threshold;
  lovrColliderGetLinearDamping(collider, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrColliderSetLinearDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping = luaL_checknumber(L, 2);
  float threshold = luaL_optnumber(L, 3, .01);
  lovrColliderSetLinearDamping(collider, damping, threshold);
  return 0;
}

int l_lovrColliderGetAngularDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping, threshold;
  lovrColliderGetAngularDamping(collider, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrColliderSetAngularDamping(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float damping = luaL_checknumber(L, 2);
  float threshold = luaL_optnumber(L, 3, .01);
  lovrColliderSetAngularDamping(collider, damping, threshold);
  return 0;
}

int l_lovrColliderApplyForce(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);

  if (lua_gettop(L) > 4) {
    float cx = luaL_checknumber(L, 5);
    float cy = luaL_checknumber(L, 6);
    float cz = luaL_checknumber(L, 7);
    lovrColliderApplyForceAtPosition(collider, x, y, z, cx, cy, cz);
  } else {
    lovrColliderApplyForce(collider, x, y, z);
  }

  return 0;
}

int l_lovrColliderApplyTorque(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrColliderApplyTorque(collider, x, y, z);
  return 0;
}

int l_lovrColliderIsKinematic(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsKinematic(collider));
  return 1;
}

int l_lovrColliderSetKinematic(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  int kinematic = lua_toboolean(L, 2);
  lovrColliderSetKinematic(collider, kinematic);
  return 0;
}

int l_lovrColliderGetLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float wx = luaL_checknumber(L, 2);
  float wy = luaL_checknumber(L, 3);
  float wz = luaL_checknumber(L, 4);
  float x, y, z;
  lovrColliderGetLocalPoint(collider, wx, wy, wz, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrColliderGetWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float wx, wy, wz;
  lovrColliderGetWorldPoint(collider, x, y, z, &wx, &wy, &wz);
  lua_pushnumber(L, wx);
  lua_pushnumber(L, wy);
  lua_pushnumber(L, wz);
  return 3;
}

int l_lovrColliderGetLocalVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float wx = luaL_checknumber(L, 2);
  float wy = luaL_checknumber(L, 3);
  float wz = luaL_checknumber(L, 4);
  float x, y, z;
  lovrColliderGetLocalVector(collider, wx, wy, wz, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrColliderGetWorldVector(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float wx, wy, wz;
  lovrColliderGetWorldVector(collider, x, y, z, &wx, &wy, &wz);
  lua_pushnumber(L, wx);
  lua_pushnumber(L, wy);
  lua_pushnumber(L, wz);
  return 3;
}

int l_lovrColliderGetLinearVelocityFromLocalPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float vx, vy, vz;
  lovrColliderGetLinearVelocityFromLocalPoint(collider, x, y, z, &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
  return 3;
}

int l_lovrColliderGetLinearVelocityFromWorldPoint(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float vx, vy, vz;
  lovrColliderGetLinearVelocityFromWorldPoint(collider, x, y, z, &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
  return 3;
}

int l_lovrColliderIsSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsSleepingAllowed(collider));
  return 1;
}

int l_lovrColliderSetSleepingAllowed(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  int allowed = lua_toboolean(L, 2);
  lovrColliderSetSleepingAllowed(collider, allowed);
  return 0;
}

int l_lovrColliderIsAwake(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushboolean(L, lovrColliderIsAwake(collider));
  return 1;
}

int l_lovrColliderSetAwake(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  int awake = lua_toboolean(L, 2);
  lovrColliderSetAwake(collider, awake);
  return 0;
}

int l_lovrColliderGetUserData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  int ref = (int) lovrColliderGetUserData(collider);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  return 1;
}

int l_lovrColliderSetUserData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  uint64_t ref = (int) lovrColliderGetUserData(collider);
  if (ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrColliderSetUserData(collider, (void*) ref);
  return 0;
}

int l_lovrColliderGetWorld(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  luax_pushtype(L, World, lovrColliderGetWorld(collider));
  return 1;
}

int l_lovrColliderGetMass(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  lua_pushnumber(L, lovrColliderGetMass(collider));
  return 1;
}

int l_lovrColliderSetMass(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float mass = luaL_checknumber(L, 2);
  lovrColliderSetMass(collider, mass);
  return 0;
}

int l_lovrColliderGetMassData(lua_State* L) {
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

int l_lovrColliderSetMassData(lua_State* L) {
  Collider* collider = luax_checktype(L, 1, Collider);
  float cx = luaL_checknumber(L, 2);
  float cy = luaL_checknumber(L, 3);
  float cz = luaL_checknumber(L, 4);
  float mass = luaL_checknumber(L, 5);
  float inertia[6];
  if (lua_istable(L, 6) && lua_objlen(L, 6) >= 6) {
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
        inertia[i] = lua_tonumber(L, i);
      } else {
        return luaL_argerror(L, i, "Expected 6 numbers or a table with 6 numbers");
      }
    }
  }
  lovrColliderSetMassData(collider, cx, cy, cz, mass, inertia);
  return 0;
}

const luaL_Reg lovrCollider[] = {
  { "addShape", l_lovrColliderAddShape },
  { "removeShape", l_lovrColliderRemoveShape },
  { "getPosition", l_lovrColliderGetPosition },
  { "setPosition", l_lovrColliderSetPosition },
  { "getOrientation", l_lovrColliderGetOrientation },
  { "setOrientation", l_lovrColliderSetOrientation },
  { "getLinearVelocity", l_lovrColliderGetLinearVelocity },
  { "setLinearVelocity", l_lovrColliderSetLinearVelocity },
  { "getAngularVelocity", l_lovrColliderGetAngularVelocity },
  { "setAngularVelocity", l_lovrColliderSetAngularVelocity },
  { "getLinearDamping", l_lovrColliderGetLinearDamping },
  { "setLinearDamping", l_lovrColliderSetLinearDamping },
  { "getAngularDamping", l_lovrColliderGetAngularDamping },
  { "setAngularDamping", l_lovrColliderSetAngularDamping },
  { "applyForce", l_lovrColliderApplyForce },
  { "applyTorque", l_lovrColliderApplyForce },
  { "isKinematic", l_lovrColliderIsKinematic },
  { "setKinematic", l_lovrColliderSetKinematic },
  { "getLocalPoint", l_lovrColliderGetLocalPoint },
  { "getWorldPoint", l_lovrColliderGetWorldPoint },
  { "getLocalVector", l_lovrColliderGetLocalVector },
  { "getWorldVector", l_lovrColliderGetWorldVector },
  { "getLinearVelocityFromLocalPoint", l_lovrColliderGetLinearVelocityFromLocalPoint },
  { "getLinearVelocityFromWorldPoint", l_lovrColliderGetLinearVelocityFromWorldPoint },
  { "isSleepingAllowed", l_lovrColliderIsSleepingAllowed },
  { "setSleepingAllowed", l_lovrColliderSetSleepingAllowed },
  { "isAwake", l_lovrColliderIsAwake },
  { "setAwake", l_lovrColliderSetAwake },
  { "getUserData", l_lovrColliderGetUserData },
  { "setUserData", l_lovrColliderSetUserData },
  { "getWorld", l_lovrColliderGetWorld },
  { "getMass", l_lovrColliderGetMass },
  { "setMass", l_lovrColliderSetMass },
  { "getMassData", l_lovrColliderGetMassData },
  { "setMassData", l_lovrColliderSetMassData },
  { NULL, NULL }
};
