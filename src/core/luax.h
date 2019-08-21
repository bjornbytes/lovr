#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdint.h>

#pragma once

struct Color;

typedef struct {
  uint32_t hash;
  void* object;
} Proxy;

#ifndef LUA_RIDX_MAINTHERAD
#define LUA_RIDX_MAINTHREAD 1
#endif

#define luax_len(L, i) (int) lua_objlen(L, i)
#define luax_registertype(L, T) _luax_registertype(L, #T, lovr ## T, lovr ## T ## Destroy)
#define luax_totype(L, i, T) (T*) _luax_totype(L, i, hash(#T))
#define luax_checktype(L, i, T) (T*) _luax_checktype(L, i, hash(#T), #T)
#define luax_pushtype(L, T, o) _luax_pushtype(L, #T, hash(#T), o)
#define luax_checkfloat(L, i) (float) luaL_checknumber(L, i)
#define luax_optfloat(L, i, x) (float) luaL_optnumber(L, i, x)
#define luax_geterror(L) lua_getfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_seterror(L) lua_setfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_clearerror(L) lua_pushnil(L), luax_seterror(L)

void _luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions, void (*destructor)(void*));
void* _luax_totype(lua_State* L, int index, uint32_t hash);
void* _luax_checktype(lua_State* L, int index, uint32_t hash, const char* debug);
void _luax_pushtype(lua_State* L, const char* name, uint32_t hash, void* object);
void luax_registerloader(lua_State* L, lua_CFunction loader, int index);
void luax_vthrow(lua_State* L, const char* format, va_list args);
void luax_traceback(lua_State* L, lua_State* T, const char* message, int level);
int luax_getstack(lua_State* L);
int luax_print(lua_State* L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
void luax_setmainthread(lua_State* L);
void luax_atexit(lua_State* L, void (*destructor)(void));
void luax_readcolor(lua_State* L, int index, struct Color* color);
