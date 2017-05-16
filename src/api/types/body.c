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

const luaL_Reg lovrBody[] = {
  { "getPosition", l_lovrBodyGetPosition },
  { "setPosition", l_lovrBodySetPosition },
  { "getOrientation", l_lovrBodyGetOrientation },
  { "setOrientation", l_lovrBodySetOrientation },
  { NULL, NULL }
};
