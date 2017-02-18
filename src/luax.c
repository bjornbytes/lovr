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

int luax_releasetype(lua_State* L) {
  lovrRelease(*(Ref**) lua_touserdata(L, 1));
  return 0;
}

// Find an object, pushing it onto the stack if it's found or leaving the stack unchanged otherwise.
int luax_getobject(lua_State* L, void* object) {
  luax_pushobjectregistry(L);
  lua_pushlightuserdata(L, object);
  lua_gettable(L, -2);

  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return 0;
  } else {
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
