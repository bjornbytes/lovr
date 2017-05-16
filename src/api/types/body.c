#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrBodyGetPosition(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x, y, z;
  lovrBodyGetPosition(body, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBodySetPosition(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBodySetPosition(body, x, y, z);
  return 0;
}

int l_lovrBodyGetOrientation(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float angle, x, y, z;
  lovrBodyGetOrientation(body, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBodySetOrientation(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float angle = luaL_checknumber(L, 2);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  lovrBodySetOrientation(body, angle, x, y, z);
  return 0;
}

int l_lovrBodyGetLinearVelocity(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x, y, z;
  lovrBodyGetLinearVelocity(body, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBodySetLinearVelocity(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBodySetLinearVelocity(body, x, y, z);
  return 0;
}

int l_lovrBodyGetAngularVelocity(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x, y, z;
  lovrBodyGetAngularVelocity(body, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBodySetAngularVelocity(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBodySetAngularVelocity(body, x, y, z);
  return 0;
}

int l_lovrBodyGetLinearDamping(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float damping, threshold;
  lovrBodyGetLinearDamping(body, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrBodySetLinearDamping(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float damping = luaL_checknumber(L, 2);
  float threshold = luaL_optnumber(L, 3, .01);
  lovrBodySetLinearDamping(body, damping, threshold);
  return 0;
}

int l_lovrBodyGetAngularDamping(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float damping, threshold;
  lovrBodyGetAngularDamping(body, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrBodySetAngularDamping(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float damping = luaL_checknumber(L, 2);
  float threshold = luaL_optnumber(L, 3, .01);
  lovrBodySetAngularDamping(body, damping, threshold);
  return 0;
}

int l_lovrBodyApplyForce(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);

  if (lua_gettop(L) > 4) {
    float cx = luaL_checknumber(L, 5);
    float cy = luaL_checknumber(L, 6);
    float cz = luaL_checknumber(L, 7);
    lovrBodyApplyForceAtPosition(body, x, y, z, cx, cy, cz);
  } else {
    lovrBodyApplyForce(body, x, y, z);
  }

  return 0;
}

int l_lovrBodyApplyTorque(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBodyApplyTorque(body, x, y, z);
  return 0;
}

int l_lovrBodyIsKinematic(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  lua_pushboolean(L, lovrBodyIsKinematic(body));
  return 1;
}

int l_lovrBodySetKinematic(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  int kinematic = lua_toboolean(L, 2);
  lovrBodySetKinematic(body, kinematic);
  return 0;
}

int l_lovrBodyGetLocalPoint(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float wx = luaL_checknumber(L, 2);
  float wy = luaL_checknumber(L, 3);
  float wz = luaL_checknumber(L, 4);
  float x, y, z;
  lovrBodyGetLocalPoint(body, wx, wy, wz, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBodyGetWorldPoint(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float wx, wy, wz;
  lovrBodyGetWorldPoint(body, x, y, z, &wx, &wy, &wz);
  lua_pushnumber(L, wx);
  lua_pushnumber(L, wy);
  lua_pushnumber(L, wz);
  return 3;
}

int l_lovrBodyGetLinearVelocityFromLocalPoint(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float vx, vy, vz;
  lovrBodyGetLinearVelocityFromLocalPoint(body, x, y, z, &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
  return 3;
}

int l_lovrBodyGetLinearVelocityFromWorldPoint(lua_State* L) {
  Body* body = luax_checktype(L, 1, Body);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  float vx, vy, vz;
  lovrBodyGetLinearVelocityFromWorldPoint(body, x, y, z, &vx, &vy, &vz);
  lua_pushnumber(L, vx);
  lua_pushnumber(L, vy);
  lua_pushnumber(L, vz);
  return 3;
}

const luaL_Reg lovrBody[] = {
  { "getPosition", l_lovrBodyGetPosition },
  { "setPosition", l_lovrBodySetPosition },
  { "getOrientation", l_lovrBodyGetOrientation },
  { "setOrientation", l_lovrBodySetOrientation },
  { "getLinearVelocity", l_lovrBodyGetLinearVelocity },
  { "setLinearVelocity", l_lovrBodySetLinearVelocity },
  { "getAngularVelocity", l_lovrBodyGetAngularVelocity },
  { "setAngularVelocity", l_lovrBodySetAngularVelocity },
  { "getLinearDamping", l_lovrBodyGetLinearDamping },
  { "setLinearDamping", l_lovrBodySetLinearDamping },
  { "getAngularDamping", l_lovrBodyGetAngularDamping },
  { "setAngularDamping", l_lovrBodySetAngularDamping },
  { "applyForce", l_lovrBodyApplyForce },
  { "applyTorque", l_lovrBodyApplyForce },
  { "isKinematic", l_lovrBodyIsKinematic },
  { "setKinematic", l_lovrBodySetKinematic },
  { "getLocalPoint", l_lovrBodyGetLocalPoint },
  { "getWorldPoint", l_lovrBodyGetWorldPoint },
  { "getLinearVelocityFromLocalPoint", l_lovrBodyGetLinearVelocityFromLocalPoint },
  { "getLinearVelocityFromWorldPoint", l_lovrBodyGetLinearVelocityFromWorldPoint },
  { NULL, NULL }
};
