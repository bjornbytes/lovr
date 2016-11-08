#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  exit(EXIT_FAILURE);
}

unsigned char* loadImage(void* data, size_t size, int* w, int* h, int* n, int channels) {
  return stbi_load_from_memory(data, size, w, h, n, channels);
}

int luaPreloadModule(lua_State* L, const char* key, lua_CFunction f) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L, f);
  lua_setfield(L, -2, key);
  lua_pop(L, 2);
  return 0;
}

void luaRegisterModule(lua_State* L, const char* name, const luaL_Reg* module) {

  // Get reference to lovr
  lua_getglobal(L, "lovr");

  // Create a table and fill it with the module functions
  lua_newtable(L);
  luaL_register(L, NULL, module);

  // lovr[name] = module
  lua_setfield(L, -2, name);

  // Pop lovr
  lua_pop(L, 1);
}

void luaRegisterType(lua_State* L, const char* name, const luaL_Reg* functions, lua_CFunction gc) {

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

  // m.__gc = gc
  if (gc) {
    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
  }

  // m.name = name
  lua_pushstring(L, name);
  lua_setfield(L, -2, "name");

  // Register class functions
  if (functions) {
    luaL_register(L, NULL, functions);
  }

  // Pop metatable
  lua_pop(L, 1);
}

// Returns a key that maps to value or NULL if the value is not in the map
const char* map_int_find(map_int_t* map, int value) {
  const char* key;
  map_iter_t iter = map_iter(map);
  while ((key = map_next(map, &iter))) {
    if (*map_get(map, key) == value) {
      return key;
    }
  }
  return NULL;
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
