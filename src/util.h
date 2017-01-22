#include "vendor/vec/vec.h"
#include "vendor/map/map.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifndef UTIL_TYPES
#define UTIL_TYPES
#define containerof(ptr, type) ((type*)((char*)(ptr) - offsetof(type, ref)))

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

#define LOVR_COLOR(r, g, b, a) ((a << 0) | (b << 8) | (g << 16) | (r << 24))
#define LOVR_COLOR_R(c) (c >> 24 & 0xff)
#define LOVR_COLOR_G(c) (c >> 16 & 0xff)
#define LOVR_COLOR_B(c) (c >> 8  & 0xff)
#define LOVR_COLOR_A(c) (c >> 0  & 0xff)

#define luax_checktype(L, i, T) *(T**) luaL_checkudata(L, i, #T)

#define luax_pushtype(L, T, x) \
  T** u = (T**) lua_newuserdata(L, sizeof(T**)); \
  luaL_getmetatable(L, #T); \
  lua_setmetatable(L, -2); \
  *u = x;

typedef struct ref {
  void (*free)(const struct ref* ref);
  int count;
} Ref;

typedef vec_t(unsigned int) vec_uint_t;
#endif

void error(const char* format, ...);
const char* map_int_find(map_int_t *map, int value);
void lovrSleep(double seconds);

void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref));
void lovrRetain(const Ref* ref);
void lovrRelease(const Ref* ref);

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f);
void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions);
int luax_releasetype(lua_State* L);
void* luax_checkenum(lua_State* L, int index, map_int_t* map, const char* typeName);
void* luax_optenum(lua_State* L, int index, const char* fallback, map_int_t* map, const char* typeName);
