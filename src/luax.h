#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lib/map/map.h"
#include "util.h"

#pragma once

#ifndef LUA_RIDX_MAINTHERAD
#define LUA_RIDX_MAINTHREAD 1
#endif

#define luax_len(L, i) (int) lua_objlen(L, i)
#define luax_totype(L, i, T) ((T*) _luax_totype(L, i, T_ ## T))
#define luax_checktype(L, i, T) ((T*) _luax_checktype(L, i, T_ ## T, #T))
#define luax_checkfloat(L, i) (float) luaL_checknumber(L, i)
#define luax_optfloat(L, i, x) (float) luaL_optnumber(L, i, x)
#define luax_geterror(L) lua_getfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_seterror(L) lua_setfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_clearerror(L) lua_pushnil(L), luax_seterror(L)
typedef void (*luax_destructor)(void);

int luax_print(lua_State* L);
void luax_setmainthread(lua_State* L);
void luax_atexit(lua_State* L, luax_destructor destructor);
void luax_registerloader(lua_State* L, lua_CFunction loader, int index);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
void luax_extendtype(lua_State* L, const char* base, const char* name, const luaL_Reg* baseFunctions, const luaL_Reg* functions);
void* _luax_totype(lua_State* L, int index, Type type);
void* _luax_checktype(lua_State* L, int index, Type type, const char* debug);
void luax_pushobject(lua_State* L, void* object);
void luax_vthrow(lua_State* L, const char* format, va_list args);
void luax_traceback(lua_State* L, lua_State* T, const char* message, int level);
int luax_getstack(lua_State* L);
int luax_getstack_panic(lua_State *L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
Color luax_checkcolor(lua_State* L, int index);
