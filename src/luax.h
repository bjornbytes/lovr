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

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f);
int luax_emptymodule(lua_State* L);
void luax_registerloader(lua_State* L, lua_CFunction loader, int index);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
void luax_extendtype(lua_State* L, const char* base, const char* name, const luaL_Reg* baseFunctions, const luaL_Reg* functions);
void* luax_testudata(lua_State* L, int index, const char* type);
void luax_pushobject(lua_State* L, void* object);
int luax_getstack(lua_State* L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
Color luax_checkcolor(lua_State* L, int index);
