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
  { NULL, NULL }
};
