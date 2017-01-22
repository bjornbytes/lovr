#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "vendor/map/map.h"

#ifndef LOVR_LUAX
#define LOVR_LUAX

#define luax_checktype(L, i, T) *(T**) luaL_checkudata(L, i, #T)
#define luax_pushtype(L, T, x) \
  T** u = (T**) lua_newuserdata(L, sizeof(T**)); \
  luaL_getmetatable(L, #T); \
  lua_setmetatable(L, -2); \
  *u = x;

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
int luax_releasetype(lua_State* L);
void luax_pushenum(lua_State* L, map_int_t* map, int value);
void* luax_checkenum(lua_State* L, int index, map_int_t* map, const char* typeName);
void* luax_optenum(lua_State* L, int index, const char* fallback, map_int_t* map, const char* typeName);

#endif
