#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  luax_pushenum(L, &ShapeTypes, lovrShapeGetType(shape));
  return 1;
}

int l_lovrShapeGetBody(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  Body* body = lovrShapeGetBody(shape);

  if (body) {
    luax_pushtype(L, Body, body);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int l_lovrShapeSetBody(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  if (lua_isnoneornil(L, 2)) {
    lovrShapeSetBody(shape, NULL);
  } else {
    Body* body = luax_checktype(L, 2, Body);
    lovrShapeSetBody(shape, body);
  }

  return 0;
}

int l_lovrShapeIsEnabled(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  lua_pushboolean(L, lovrShapeIsEnabled(shape));
  return 1;
}

int l_lovrShapeSetEnabled(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  int enabled = lua_toboolean(L, 2);
  lovrShapeSetEnabled(shape, enabled);
  return 0;
}

int l_lovrShapeGetUserData(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  int ref = (int) lovrShapeGetUserData(shape);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  return 1;
}

int l_lovrShapeSetUserData(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint64_t ref = (int) lovrShapeGetUserData(shape);
  if (ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrShapeSetUserData(shape, (void*) ref);
  return 0;
}
