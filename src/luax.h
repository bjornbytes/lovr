#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lib/map/map.h"
#include "util.h"

#pragma once

#define STRINGIFY(x) #x

#define luax_totype(L, i, T) (luax_testudata(L, i, #T))
#define luax_checktype(L, i, T) *(T**) luaL_checkudata(L, i, #T)
#define luax_checktypeof(L, i, T) \
  *(T**) (luaL_argcheck(L, lua_touserdata(L, i), i, "Expected " STRINGIFY(T)), \
  lua_getmetatable(L, i), \
  lua_getfield(L, -1, "name"), \
  lua_getfield(L, -2, "super"), \
  lua_pushstring(L, #T), \
  luaL_argcheck(L, lua_equal(L, -1, -2) || lua_equal(L, -1, -3), i, "Expected " STRINGIFY(T)), \
  lua_pop(L, 4), \
  lua_touserdata(L, i))
#define luax_newobject(L, T, x) \
  T** u = (T**) lua_newuserdata(L, sizeof(T**)); \
  luax_registerobject(L, x); \
  luaL_getmetatable(L, #T); \
  lua_setmetatable(L, -2); \
  *u = x;
#define luax_pushtype(L, T, x) \
  if (!x) { lua_pushnil(L); } \
  else if (!luax_getobject(L, x)) { luax_newobject(L, T, x); }

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
void luax_extendtype(lua_State* L, const char* base, const char* name, const luaL_Reg* baseFunctions, const luaL_Reg* functions);
int luax_releasetype(lua_State* L);
void* luax_testudata(lua_State* L, int index, const char* type);
int luax_getobject(lua_State* L, void* object);
void luax_registerobject(lua_State* L, void* object);
void luax_pushconf(lua_State* L);
void luax_setconf(lua_State* L);
void luax_pushenum(lua_State* L, map_int_t* map, int value);
void* luax_checkenum(lua_State* L, int index, map_int_t* map, const char* typeName);
void* luax_optenum(lua_State* L, int index, const char* fallback, map_int_t* map, const char* typeName);
Color luax_checkcolor(lua_State* L, int index);
