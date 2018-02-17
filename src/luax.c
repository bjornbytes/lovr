#include "luax.h"
#include "util.h"
#include <stdlib.h>

static int luax_pushobjectname(lua_State* L) {
  lua_getfield(L, -1, "name");
  return 1;
}

static void luax_pushobjectregistry(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrobjects");

  // Create the registry if it doesn't exist yet
  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // Create the metatable
    lua_newtable(L);

    // __mode = v
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");

    // Set the metatable
    lua_setmetatable(L, -2);

    // Write the table to the registry
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrobjects");
  }
}

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L, f);
  lua_setfield(L, -2, key);
  lua_pop(L, 2);
  return 0;
}

void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions) {

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

  // m.__gc = gc
  lua_pushcfunction(L, luax_releasetype);
  lua_setfield(L, -2, "__gc");

  // m.name = name
  lua_pushstring(L, name);
  lua_setfield(L, -2, "name");

  // m.__tostring
  lua_pushcfunction(L, luax_pushobjectname);
  lua_setfield(L, -2, "__tostring");

  // Register class functions
  if (functions) {
    luaL_register(L, NULL, functions);
  }

  // Pop metatable
  lua_pop(L, 1);
}

void luax_extendtype(lua_State* L, const char* base, const char* name, const luaL_Reg* baseFunctions, const luaL_Reg* functions) {
  luax_registertype(L, name, functions);
  luaL_getmetatable(L, name);

  lua_pushstring(L, base);
  lua_setfield(L, -2, "super");

  if (baseFunctions) {
    luaL_register(L, NULL, baseFunctions);
  }

  lua_pop(L, 1);
}

int luax_releasetype(lua_State* L) {
  lovrRelease(*(Ref**) lua_touserdata(L, 1));
  return 0;
}

void* luax_testudata(lua_State* L, int index, const char* type) {
  void* p = lua_touserdata(L, index);

  if (!p || !lua_getmetatable(L, index)) {
    return NULL;
  }

  luaL_getmetatable(L, type);
  int equal = lua_rawequal(L, -1, -2);
  lua_pop(L, 2);
  return equal ? p : NULL;
}

// Find an object, pushing it onto the stack if it's found or leaving the stack unchanged otherwise.
int luax_getobject(lua_State* L, void* object) {
  luax_pushobjectregistry(L);
  lua_pushlightuserdata(L, object);
  lua_gettable(L, -2);

  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);
    return 0;
  } else {
    lua_remove(L, -2);
    return 1;
  }
}

// Registers the userdata on the top of the stack in the object registry.
void luax_registerobject(lua_State* L, void* object) {
  luax_pushobjectregistry(L);
  lua_pushlightuserdata(L, object);
  lua_pushvalue(L, -3);
  lua_settable(L, -3);
  lua_pop(L, 1);
  lovrRetain(object);
}

int luax_getstack(lua_State* L) {
  const char* message = luaL_checkstring(L, -1);
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  lua_pushstring(L, message);
  lua_pushinteger(L, 2);
  lua_call(L, 2, 1);
  return 1;
}

void luax_pushconf(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrconf");
}

int luax_setconf(lua_State* L) {
  luax_pushconf(L);
  lovrAssert(lua_isnil(L, -1), "Unable to set lovr.conf multiple times");
  lua_pop(L, 1);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrconf");
  return 0;
}

void luax_pushenum(lua_State* L, map_int_t* map, int value) {
  const char* key;
  map_iter_t iter = map_iter(map);
  while ((key = map_next(map, &iter))) {
    if (*map_get(map, key) == value) {
      lua_pushstring(L, key);
      return;
    }
  }
  lua_pushnil(L);
}

void* luax_checkenum(lua_State* L, int index, map_int_t* map, const char* typeName) {
  const char* key = luaL_checkstring(L, index);
  void* value = map_get(map, key);
  if (!value) {
    luaL_error(L, "Invalid %s '%s'", typeName, key);
    return NULL;
  }

  return value;
}

void* luax_optenum(lua_State* L, int index, const char* fallback, map_int_t* map, const char* typeName) {
  const char* key = luaL_optstring(L, index, fallback);
  void* value = map_get(map, key);
  if (!value) {
    luaL_error(L, "Invalid %s '%s'", typeName, key);
    return NULL;
  }

  return value;
}

Color luax_checkcolor(lua_State* L, int index) {
  Color color = { 1., 1., 1., 1. };

  if (lua_istable(L, 1)) {
    for (int i = 1; i <= 4; i++) {
      lua_rawgeti(L, 1, i);
    }
    color.r = luaL_checknumber(L, -4);
    color.g = luaL_checknumber(L, -3);
    color.b = luaL_checknumber(L, -2);
    color.a = luaL_optnumber(L, -1, 1);
    lua_pop(L, 4);
  } else if (lua_gettop(L) >= index + 2) {
    color.r = luaL_checknumber(L, index);
    color.g = luaL_checknumber(L, index + 1);
    color.b = luaL_checknumber(L, index + 2);
    color.a = luaL_optnumber(L, index + 3, 1);
  } else {
    luaL_error(L, "Invalid color, expected 3 numbers, 4 numbers, or a table");
  }

  return color;
}
