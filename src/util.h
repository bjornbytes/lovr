#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "vendor/vec/vec.h"

#ifndef UTIL_TYPES
#define UTIL_TYPES
#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

typedef vec_t(unsigned int) vec_uint_t;
#endif

void error(const char* format, ...);
unsigned char* loadImage(void* data, size_t size, int* w, int* h, int* n, int channels);
void luaRegisterModule(lua_State* L, const char* name, const luaL_Reg* module);
void luaRegisterType(lua_State* L, const char* name, const luaL_Reg* functions, lua_CFunction gc);
int luaPreloadModule(lua_State* L, const char* key, lua_CFunction f);
