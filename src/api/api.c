#include "api.h"
#include "core/os.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifndef LOVR_DISABLE_GRAPHICS
#include "graphics/model.h"
#endif

typedef void voidFn(void);
typedef void destructorFn(void*);

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#else
#define LOVR_EXPORT __attribute__((visibility("default")))
#endif

LOVR_EXPORT int luaopen_lovr(lua_State* L);
LOVR_EXPORT int luaopen_lovr_audio(lua_State* L);
LOVR_EXPORT int luaopen_lovr_data(lua_State* L);
LOVR_EXPORT int luaopen_lovr_event(lua_State* L);
LOVR_EXPORT int luaopen_lovr_filesystem(lua_State* L);
LOVR_EXPORT int luaopen_lovr_graphics(lua_State* L);
LOVR_EXPORT int luaopen_lovr_headset(lua_State* L);
LOVR_EXPORT int luaopen_lovr_math(lua_State* L);
LOVR_EXPORT int luaopen_lovr_physics(lua_State* L);
LOVR_EXPORT int luaopen_lovr_system(lua_State* L);
LOVR_EXPORT int luaopen_lovr_thread(lua_State* L);
LOVR_EXPORT int luaopen_lovr_timer(lua_State* L);

// Object names are lightuserdata because Variants need a non-Lua string due to threads.
static int luax_meta__tostring(lua_State* L) {
  lua_getfield(L, -1, "__info");
  TypeInfo* info = lua_touserdata(L, -1);
  lua_pushstring(L, info->name);
  return 1;
}

static int luax_meta__gc(lua_State* L) {
  Proxy* p = lua_touserdata(L, 1);
  if (p) {
    lua_getmetatable(L, 1);
    lua_getfield(L, -1, "__info");
    TypeInfo* info = lua_touserdata(L, -1);
    if (info->destructor) {
      lovrRelease(p->object, info->destructor);
      p->object = NULL;
    }
  }
  return 0;
}

static int luax_module__gc(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrmodules");
  for (int i = luax_len(L, 2); i >= 1; i--) {
    lua_rawgeti(L, 2, i);
    voidFn* destructor = (voidFn*) lua_tocfunction(L, -1);
    destructor();
    lua_pop(L, 1);
  }
  return 0;
}

void luax_preload(lua_State* L) {
  static const luaL_Reg lovrModules[] = {
    { "lovr", luaopen_lovr },
#ifndef LOVR_DISABLE_AUDIO
    { "lovr.audio", luaopen_lovr_audio },
#endif
#ifndef LOVR_DISABLE_DATA
    { "lovr.data", luaopen_lovr_data },
#endif
#ifndef LOVR_DISABLE_EVENT
    { "lovr.event", luaopen_lovr_event },
#endif
#ifndef LOVR_DISABLE_FILESYSTEM
    { "lovr.filesystem", luaopen_lovr_filesystem },
#endif
#ifndef LOVR_DISABLE_GRAPHICS
    { "lovr.graphics", luaopen_lovr_graphics },
#endif
#ifndef LOVR_DISABLE_HEADSET
    { "lovr.headset", luaopen_lovr_headset },
#endif
#ifndef LOVR_DISABLE_MATH
    { "lovr.math", luaopen_lovr_math },
#endif
#ifndef LOVR_DISABLE_PHYSICS
    { "lovr.physics", luaopen_lovr_physics },
#endif
#ifndef LOVR_DISABLE_SYSTEM
    { "lovr.system", luaopen_lovr_system },
#endif
#ifndef LOVR_DISABLE_THREAD
    { "lovr.thread", luaopen_lovr_thread },
#endif
#ifndef LOVR_DISABLE_TIMER
    { "lovr.timer", luaopen_lovr_timer },
#endif
    { NULL, NULL }
  };

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luax_register(L, lovrModules);
  lua_pop(L, 2);
}

void _luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions, destructorFn* destructor) {

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

  // m.__info = info
  TypeInfo* info = lua_newuserdata(L, sizeof(TypeInfo));
  info->name = name;
  info->destructor = destructor;
  lua_setfield(L, -2, "__info");

  // m.__gc = gc
  lua_pushcfunction(L, luax_meta__gc);
  lua_setfield(L, -2, "__gc");

  // m.__tostring
  lua_pushcfunction(L, luax_meta__tostring);
  lua_setfield(L, -2, "__tostring");

  // Register class functions
  if (functions) {
    luax_register(L, functions);
  }

  // :release function
  lua_pushcfunction(L, luax_meta__gc);
  lua_setfield(L, -2, "release");

  // Pop metatable
  lua_pop(L, 1);
}

void* _luax_totype(lua_State* L, int index, uint64_t hash) {
  Proxy* p = lua_touserdata(L, index);

  if (p && lua_type(L, index) != LUA_TLIGHTUSERDATA && p->hash == hash) {
    return p->object;
  }

  return NULL;
}

void* _luax_checktype(lua_State* L, int index, uint64_t hash, const char* debug) {
  void* object = _luax_totype(L, index, hash);

  if (!object) {
    luax_typeerror(L, index, debug);
  }

  return object;
}

int luax_typeerror(lua_State* L, int index, const char* expected) {
  const char* name;
  if (luaL_getmetafield(L, index, "__name") == LUA_TSTRING) {
    name = lua_tostring(L, -1);
  } else if (lua_type(L, index) == LUA_TLIGHTUSERDATA) {
    name = "light userdata";
  } else {
    name = luaL_typename(L, index);
  }
  const char* message = lua_pushfstring(L, "%s expected, got %s", expected, name);
  return luaL_argerror(L, index, message);
}

// Registers the userdata on the top of the stack in the registry.
void _luax_pushtype(lua_State* L, const char* type, uint64_t hash, void* object) {
  if (!object) {
    lua_pushnil(L);
    return;
  }

  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrobjects");

  // Create the registry if it doesn't exist yet
  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // Create the metatable
    lua_newtable(L);

    // __mode = v
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");

    // Set the metatable
    lua_setmetatable(L, -2);

    // Write the table to the registry
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrobjects");
  }

  lua_pushlightuserdata(L, object);
  lua_gettable(L, -2);

  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
  } else {
    lua_remove(L, -2);
    return;
  }

  // Allocate userdata
  Proxy* p = (Proxy*) lua_newuserdata(L, sizeof(Proxy));
  luaL_getmetatable(L, type);
  lovrAssert(lua_istable(L, -1), "Unknown type '%s' (maybe its module needs to be required)", type);
  lua_setmetatable(L, -2);
  lovrRetain(object);
  p->object = object;
  p->hash = hash;

  // Write to registry and remove registry, leaving userdata on stack
  lua_pushlightuserdata(L, object);
  lua_pushvalue(L, -2);
  lua_settable(L, -4);
  lua_remove(L, -2);
}

int _luax_checkenum(lua_State* L, int index, const StringEntry* map, const char* fallback, const char* label) {
  size_t length;
  const char* string = fallback ? luaL_optlstring(L, index, fallback, &length) : luaL_checklstring(L, index, &length);

  for (int i = 0; map[i].length; i++) {
    if (map[i].length == length && !memcmp(map[i].string, string, length)) {
      return i;
    }
  }

  if (index > 0) {
    return luaL_argerror(L, index, lua_pushfstring(L, "invalid %s '%s'", label, string));
  } else {
    return luaL_error(L, "invalid %s '%s'", label, string);
  }
}

void luax_registerloader(lua_State* L, lua_CFunction loader, int index) {
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "insert");
  lua_getglobal(L, "package");
#if LUA_VERSION_NUM == 501
  lua_getfield(L, -1, "loaders");
#else
  lua_getfield(L, -1, "searchers");
#endif
  lua_remove(L, -2);
  if (lua_istable(L, -1)) {
    lua_pushinteger(L, index);
    lua_pushcfunction(L, loader);
    lua_call(L, 3, 0);
  } else {
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
}

int luax_resume(lua_State* T, int n) {
#if LUA_VERSION_NUM >= 504
  int results;
  return lua_resume(T, NULL, n, &results);
#elif LUA_VERSION_NUM >= 502
  return lua_resume(T, NULL, n);
#else
  return lua_resume(T, n);
#endif
}

void luax_vthrow(void* context, const char* format, va_list args) {
  lua_State* L = (lua_State*) context;
  lua_pushvfstring(L, format, args);
  lua_error(L);
}

void luax_vlog(void* context, int level, const char* tag, const char* format, va_list args) {
  static const char* levels[] = {
    [LOG_DEBUG] = "debug",
    [LOG_INFO] = "info",
    [LOG_WARN] = "warn",
    [LOG_ERROR] = "error"
  };
  lua_State* L = (lua_State*) context;
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "log");
  if (lua_type(L, -1) == LUA_TFUNCTION) {
    lua_pushvfstring(L, format, args);
    lua_pushstring(L, levels[level]);
    lua_pushstring(L, tag);
    lua_call(L, 3, 0);
  }
  lua_pop(L, 1);
}

// An implementation of luaL_traceback for Lua 5.1
void luax_traceback(lua_State* L, lua_State* T, const char* message, int level) {
  if (!lua_checkstack(L, 5)) {
    return;
  }

  lua_getglobal(L, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return;
  }

  lua_remove(L, -2);
  lua_pushthread(T);
  lua_pushstring(L, message);
  lua_pushinteger(L, level);
  lua_call(L, 3, 1);
}

int luax_getstack(lua_State *L) {
  luax_traceback(L, L, lua_tostring(L, 1), 2);
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

void luax_setmainthread(lua_State *L) {
#if LUA_VERSION_NUM < 502
  lua_pushthread(L);
  lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
#endif
}

void luax_atexit(lua_State* L, voidFn* destructor) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrmodules");

  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // Userdata sentinel since tables don't have __gc (yet)
    lua_newuserdata(L, sizeof(void*));
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, luax_module__gc);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "");

    // Write to the registry
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrmodules");
  }

  int length = luax_len(L, -1);
  lua_pushcfunction(L, (lua_CFunction) destructor);
  lua_rawseti(L, -2, length + 1);
  lua_pop(L, 1);
}

void luax_readcolor(lua_State* L, int index, Color* color) {
  color->r = color->g = color->b = color->a = 1.f;

  if (lua_istable(L, index)) {
    for (int i = 1; i <= 4; i++) {
      lua_rawgeti(L, index, i);
    }
    color->r = luax_checkfloat(L, -4);
    color->g = luax_checkfloat(L, -3);
    color->b = luax_checkfloat(L, -2);
    color->a = luax_optfloat(L, -1, 1.);
    lua_pop(L, 4);
  } else if (lua_gettop(L) >= index + 2) {
    color->r = luax_checkfloat(L, index);
    color->g = luax_checkfloat(L, index + 1);
    color->b = luax_checkfloat(L, index + 2);
    color->a = luax_optfloat(L, index + 3, 1.);
  } else if (lua_gettop(L) == index) {
    uint32_t x = luaL_checkinteger(L, index);
    color->r = ((x >> 16) & 0xff) / 255.f;
    color->g = ((x >> 8) & 0xff) / 255.f;
    color->b = ((x >> 0) & 0xff) / 255.f;
    color->a = 1.f;
  } else {
    luaL_error(L, "Invalid color, expected a hexcode, 3 numbers, 4 numbers, or a table");
  }
}

int luax_readmesh(lua_State* L, int index, float** vertices, uint32_t* vertexCount, uint32_t** indices, uint32_t* indexCount, bool* shouldFree) {
  if (lua_istable(L, index)) {
    luaL_checktype(L, index + 1, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    bool nested = lua_type(L, -1) == LUA_TTABLE;
    lua_pop(L, 1);

    *vertexCount = luax_len(L, index) / (nested ? 1 : 3);
    *indexCount = luax_len(L, index + 1);
    lovrAssert(*indexCount % 3 == 0, "Index count must be a multiple of 3");
    *vertices = malloc(sizeof(float) * *vertexCount * 3);
    *indices = malloc(sizeof(uint32_t) * *indexCount);
    lovrAssert(vertices && indices, "Out of memory");
    *shouldFree = true;

    if (nested) {
      for (uint32_t i = 0; i < *vertexCount; i++) {
        lua_rawgeti(L, index, i + 1);
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        lua_rawgeti(L, -3, 3);
        (*vertices)[3 * i + 0] = luax_checkfloat(L, -3);
        (*vertices)[3 * i + 1] = luax_checkfloat(L, -2);
        (*vertices)[3 * i + 2] = luax_checkfloat(L, -1);
        lua_pop(L, 4);
      }
    } else {
      for (uint32_t i = 0; i < *vertexCount * 3; i++) {
        lua_rawgeti(L, index, i + 1);
        (*vertices)[i] = luax_checkfloat(L, -1);
        lua_pop(L, 1);
      }
    }

    for (uint32_t i = 0; i < *indexCount; i++) {
      lua_rawgeti(L, index + 1, i + 1);
      uint32_t index = luaL_checkinteger(L, -1) - 1;
      lovrAssert(index < *vertexCount, "Invalid vertex index %d (expected [%d, %d])", 1, *vertexCount);
      (*indices)[i] = index;
      lua_pop(L, 1);
    }

    return index + 2;
  }

#ifndef LOVR_DISABLE_GRAPHICS
  Model* model = luax_totype(L, index, Model);
  if (model) {
    lovrModelGetTriangles(model, vertices, vertexCount, indices, indexCount);
    *shouldFree = false;
    return index + 1;
  }
#endif

  return luaL_argerror(L, index, "table or Model");
}
