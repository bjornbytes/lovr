#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lib/map/map.h"
#include "util.h"

#pragma once

#define luax_totype(L, i, T) ((T*) _luax_totype(L, i, #T))
#define luax_checktype(L, i, T) ((T*) _luax_checktype(L, i, #T))

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f);
void luax_registerloader(lua_State* L, lua_CFunction loader, int index);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
void luax_extendtype(lua_State* L, const char* base, const char* name, const luaL_Reg* baseFunctions, const luaL_Reg* functions);
void* _luax_totype(lua_State* L, int index, const char* type);
void* _luax_checktype(lua_State* L, int index, const char* type);
void luax_pushobject(lua_State* L, void* object);
int luax_getstack(lua_State* L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
Color luax_checkcolor(lua_State* L, int index);
