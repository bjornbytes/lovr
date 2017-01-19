#include "util.h"
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  exit(EXIT_FAILURE);
}

unsigned char* loadImage(void* data, size_t size, int* w, int* h, int* n, int channels) {
  stbi_set_flip_vertically_on_load(1);
  return stbi_load_from_memory(data, size, w, h, n, channels);
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

void lovrSleep(double seconds) {
#ifdef _WIN32
  Sleep((unsigned int)(seconds * 1000));
#else
  usleep((unsigned int)(seconds * 1000000));
#endif
}

void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref)) {
  void* object = malloc(size);
  if (!object) return NULL;
  *((Ref*) object) = (Ref) { destructor, 1 };
  return object;
}

void lovrRetain(const Ref* ref) {
  ((Ref*) ref)->count++;
}

void lovrRelease(const Ref* ref) {
  if (--((Ref*) ref)->count == 0 && ref->free) ref->free(ref);
}

int luax_preloadmodule(lua_State* L, const char* key, lua_CFunction f) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L, f);
  lua_setfield(L, -2, key);
  lua_pop(L, 2);
  return 0;
}

static int luax_pushobjectname(lua_State* L) {
  lua_getfield(L, -1, "name");
  return 1;
}

void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions, lua_CFunction gc) {

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

int luax_istype(lua_State* L, int index, const char* name) {
  if (lua_getmetatable(L, index)) {
    luaL_getmetatable(L, name);
    int equal = lua_equal(L, -1, -2);
    lua_pop(L, 2);
    return equal;
  }

  return 0;
}

void* luax_checkenum(lua_State* L, int index, map_int_t* map, const char* debug) {
  const char* key = luaL_checkstring(L, index);
  void* value = map_get(map, key);
  if (!value) {
    luaL_error(L, "Invalid %s '%s'", debug, key);
    return NULL;
  }

  return value;
}

void* luax_optenum(lua_State* L, int index, const char* fallback, map_int_t* map, const char* debug) {
  const char* key = luaL_optstring(L, index, fallback);
  void* value = map_get(map, key);
  if (!value) {
    luaL_error(L, "Invalid %s '%s'", debug, key);
    return NULL;
  }

  return value;
}
