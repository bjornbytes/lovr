#include "api.h"
#include "math/math.h"
#include "core/maf.h"
#include "util.h"
#ifdef LOVR_USE_LUAU
#include <luacode.h>
#endif
#include "lib/luax/lutf8lib.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifndef LOVR_DISABLE_GRAPHICS
#include "data/modelData.h"
#include "graphics/graphics.h"
#endif

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
LOVR_EXPORT int luaopen_lovr_task(lua_State* L);
LOVR_EXPORT int luaopen_lovr_thread(lua_State* L);
LOVR_EXPORT int luaopen_lovr_timer(lua_State* L);

static int luax_tostring(lua_State* L) {
  Object* object = lua_touserdata(L, 1);
  lua_pushfstring(L, "%s: %p", lovrTypeInfo[object->type].name, object->pointer);
  return 1;
}

static int luax_type(lua_State* L) {
  Object* object = lua_touserdata(L, -1);
  lua_pushstring(L, lovrTypeInfo[object->type].name);
  return 1;
}

#ifdef LOVR_USE_LUAU
static void luax_destructor(lua_State* L, void* userdata) {
  Object* object = userdata;
  if (object->pointer) {
    lovrRelease(object->pointer, lovrTypeInfo[object->type].destructor);
    object->pointer = NULL;
  }
}
#endif

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
#ifndef LOVR_DISABLE_TASK
    { "lovr.task", luaopen_lovr_task },
#endif
#ifndef LOVR_DISABLE_THREAD
    { "lovr.thread", luaopen_lovr_thread },
#endif
#ifndef LOVR_DISABLE_TIMER
    { "lovr.timer", luaopen_lovr_timer },
#endif
#ifndef LOVR_DISABLE_UTF8
    { "utf8", luaopen_utf8 },
#endif
    { NULL, NULL }
  };

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luax_register(L, lovrModules);
  lua_pop(L, 2);
}

static int luax_release(lua_State* L) {
  Object* object = lua_touserdata(L, 1);

  if (!object) {
    return 0;
  }

  // Remove from userdata cache
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrobjects");
  lua_pushlightuserdata(L, object->pointer);
  lua_pushnil(L);
  lua_rawset(L, -3);
  lua_pop(L, 1);

  // Release
  lovrRelease(object->pointer, lovrTypeInfo[object->type].destructor);
  object->pointer = NULL;

  return 0;
}

void _luax_registertype(lua_State* L, int type, const char* name, void (*destructor)(void*), const luaL_Reg* functions) {
  lovrTypeInfo[type] = (TypeInfo) { name, destructor };

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

#ifdef LOVR_USE_LUAU
  lua_setuserdatadtor(L, type, luax_destructor);
  lua_pushvalue(L, -1);
  lua_setuserdatametatable(L, type);
#else
  // m.__gc = luax_release
  lua_pushcfunction(L, luax_release);
  lua_setfield(L, -2, "__gc");

  // m.__close = luax_release
  lua_pushcfunction(L, luax_release);
  lua_setfield(L, -2, "__close");
#endif

  // m.__tostring
  lua_pushcfunction(L, luax_tostring);
  lua_setfield(L, -2, "__tostring");

  // :release method
  lua_pushcfunction(L, luax_release);
  lua_setfield(L, -2, "release");

  // :type method
  lua_pushcfunction(L, luax_type);
  lua_setfield(L, -2, "type");

  // Register methods
  if (functions) {
    luax_register(L, functions);
  }

  // Pop metatable
  lua_pop(L, 1);
}

void* _luax_totype(lua_State* L, int index, int type) {
#ifdef LOVR_USE_LUAU
  Object* object = lua_touserdatatagged(L, index, type);
  return object ? object->pointer : NULL;
#else
  Object* object = lua_touserdata(L, index);

  if (object && lua_type(L, index) != LUA_TLIGHTUSERDATA && object->type == type) {
    return object->pointer;
  }

  return NULL;
#endif
}

void* _luax_checktype(lua_State* L, int index, int type) {
  void* object = _luax_totype(L, index, type);

  if (!object) {
    luax_typeerror(L, index, lovrTypeInfo[type].name);
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
void _luax_pushtype(lua_State* L, int type, void* pointer) {
  if (!pointer) {
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

  lua_pushlightuserdata(L, pointer);
  lua_gettable(L, -2);

  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
  } else {
    lua_remove(L, -2);
    return;
  }

  // Allocate userdata
#ifdef LOVR_USE_LUAU
  Object* object = (Object*) lua_newuserdatataggedwithmetatable(L, sizeof(Object), type);
#else
  Object* object = (Object*) lua_newuserdata(L, sizeof(Object));
  luaL_newmetatable(L, lovrTypeInfo[type].name);
  lua_setmetatable(L, -2);
#endif
  lovrRetain(pointer);
  object->pointer = pointer;
  object->type = type;

  // Write to registry and remove registry, leaving userdata on stack
  lua_pushlightuserdata(L, pointer);
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

  return 0;
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
#elif LUA_VERSION_NUM >= 502 || defined(LOVR_USE_LUAU)
  return lua_resume(T, NULL, n);
#else
  return lua_resume(T, n);
#endif
}

int luax_loadbufferx(lua_State* L, const char* buffer, size_t size, const char* name, const char* mode) {
#ifdef LOVR_USE_LUAU
  size_t bytecodeSize = 0;
  char* bytecode = luau_compile(buffer, size, NULL, &bytecodeSize);
  int result = luau_load(L, name, bytecode, bytecodeSize, 0);
  free(bytecode);
  return result ? LUA_ERRSYNTAX : LUA_OK;
#elif LUA_VERSION_NUM >= 502
  return luaL_loadbufferx(L, buffer, size, name, mode);
#else
  bool binary = buffer[0] == LUA_SIGNATURE[0];
  if (mode && !strchr(mode, binary ? 'b' : 't')) {
    lua_pushliteral(L, "attempt to load chunk with wrong mode");
    return LUA_ERRSYNTAX;
  }
  return luaL_loadbuffer(L, buffer, size, name);
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
  } else {
    lua_pop(L, 1);
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

int luax_pushsuccess(lua_State* L, bool success) {
  if (success) {
    lua_pushboolean(L, true);
    return 1;
  } else {
    lua_pushboolean(L, false);
    lua_pushstring(L, lovrGetError());
    return 2;
  }
}

void luax_pushconf(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrconf");
}

int luax_setconf(lua_State* L) {
  luax_pushconf(L);
  luax_check(L, lua_isnil(L, -1), "Unable to set lovr.conf multiple times");
  lua_pop(L, 1);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrconf");
  return 0;
}

void luax_pushstash(lua_State* L, const char* name) {
  lua_getfield(L, LUA_REGISTRYINDEX, name);

  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // metatable
    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, name);
  }
}

void* luax_getthreaddata(lua_State* L) {
#ifdef LOVR_USE_LUAU
  return lua_getthreaddata(L);
#elif LUA_VERSION_NUM >= 503
  return *(void**) lua_getextraspace(L);
#else
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrthreaddata");
  if (lua_isnil(L, -1)) return lua_pop(L, 1), NULL;
  lua_pushthread(L);
  lua_rawget(L, -2);
  void* data = lua_touserdata(L, -1);
  lua_pop(L, 2);
  return data;
#endif
}

void luax_setthreaddata(lua_State* L, void* data) {
#ifdef LOVR_USE_LUAU
  lua_setthreaddata(L, data);
#elif LUA_VERSION_NUM >= 503
  *(void**) lua_getextraspace(L) = data;
#else
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrthreaddata");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);

    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrthreaddata");
  }
  lua_pushthread(L);
  if (data) {
    lua_pushlightuserdata(L, data);
  } else {
    lua_pushnil(L);
  }
  lua_rawset(L, -3);
  lua_pop(L, 1);
#endif
}

void luax_setmainthread(lua_State *L) {
#if LUA_VERSION_NUM < 502
  lua_pushthread(L);
  lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
#endif
}

typedef struct Finalizer {
  struct Finalizer* next;
  void (*fn)(void);
} Finalizer;

void luax_atexit(lua_State* L, void (*fn)(void)) {
  Finalizer* finalizer = lovrMalloc(sizeof(Finalizer));
  finalizer->fn = fn;

  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrfinalizers");
  finalizer->next = lua_touserdata(L, -1);
  lua_pop(L, 1);

  lua_pushlightuserdata(L, finalizer);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrfinalizers");
}

void luax_close(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrfinalizers");
  Finalizer* finalizer = lua_touserdata(L, -1);

  lua_close(L);

  while (finalizer) {
    finalizer->fn();
    Finalizer* next = finalizer->next;
    lovrFree(finalizer);
    finalizer = next;
  }
}

uint32_t _luax_checku32(lua_State* L, int index) {
  double x = lua_tonumber(L, index);

  if (x == 0. && !lua_isnumber(L, index)) {
    luax_typeerror(L, index, "number");
  }

  if (x < 0. || x > UINT32_MAX) {
    const char* message = lua_pushfstring(L, "expected a number between 0 and %u, got %g", UINT32_MAX, x);
    luaL_argerror(L, index, message);
  }

  return (uint32_t) x;
}

uint32_t _luax_optu32(lua_State* L, int index, uint32_t fallback) {
  return luaL_opt(L, luax_checku32, index, fallback);
}

static void _luax_checkvariant(lua_State* L, int index, Variant* variant, int depth) {
  luax_check(L, depth <= 128, "Table contains cycles!");

  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
    case LUA_TNONE:
      variant->type = TYPE_NIL;
      break;

    case LUA_TBOOLEAN:
      variant->type = TYPE_BOOLEAN;
      variant->boolean.value = lua_toboolean(L, index);
      break;

    case LUA_TNUMBER:
      variant->type = TYPE_NUMBER;
      variant->number.value = lua_tonumber(L, index);
      break;

    case LUA_TSTRING: {
      size_t length;
      const char* string = lua_tolstring(L, index, &length);
      if (length <= sizeof(variant->ministring.data)) {
        variant->type = TYPE_MINISTRING;
        variant->ministring.length = (uint8_t) length;
        memcpy(variant->ministring.data, string, length);
      } else {
        variant->type = TYPE_STRING;
        variant->string.pointer = lovrMalloc(length + 1);
        memcpy(variant->string.pointer, string, length);
        variant->string.pointer[length] = '\0';
        variant->string.length = length;
      }
      break;
    }

    case LUA_TUSERDATA:
      variant->type = TYPE_OBJECT;
      Object* object = lua_touserdata(L, index);
      variant->object.type = object->type;
      variant->object.pointer = object->pointer;
      lovrRetain(object->pointer);
      break;

    case LUA_TLIGHTUSERDATA:
      variant->type = TYPE_POINTER;
      variant->pointer.value = lua_touserdata(L, index);
      break;

    case LUA_TTABLE:
      if (index < 0) { index += lua_gettop(L) + 1; }
      luaL_checkstack(L, 2, "Lua stack overflow when serializing table (maybe it contains a cycle?)");

      lua_pushnil(L);
      size_t count = 0;
      while (lua_next(L, index) != 0) {
        count++;
        lua_pop(L, 1);
      }

      variant->type = TYPE_TABLE;
      variant->table.count = count;

      if (count == 0) {
        variant->table.pairs = NULL;
        break;
      } else {
        variant->table.pairs = lovrMalloc(count * 2 * sizeof(Variant));

        int i = 0;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          _luax_checkvariant(L, -1, &variant->table.pairs[2 * i + 1], depth + 1);
          lua_pop(L, 1);
          _luax_checkvariant(L, -1, &variant->table.pairs[2 * i + 0], depth + 1);
          i++;
        }
      }
      break;

#ifdef LOVR_USE_LUAU
    case LUA_TVECTOR: {
      const float* v = lua_tovector(L, index);
      variant->type = TYPE_VECTOR;
      memcpy(variant->vector.data, v, 3 * sizeof(float));
      break;
    }

    case LUA_TQUATERNION: {
      const short* q = lua_toquaternion(L, index);
      variant->type = TYPE_QUATERNION;
      memcpy(variant->quaternion.data, q, 4 * sizeof(int16_t));
      break;
    }
#endif

    default:
      luaL_error(L, "Bad variant type for argument %d: %s", index, lua_typename(L, type));
      return;
  }
}

void luax_checkvariant(lua_State* L, int index, Variant* variant) {
  _luax_checkvariant(L, index, variant, 0);
}

int luax_pushvariant(lua_State* L, Variant* variant) {
  switch (variant->type) {
    case TYPE_NIL: lua_pushnil(L); return 1;
    case TYPE_BOOLEAN: lua_pushboolean(L, variant->boolean.value); return 1;
    case TYPE_NUMBER: lua_pushnumber(L, variant->number.value); return 1;
    case TYPE_STRING: lua_pushlstring(L, variant->string.pointer, variant->string.length); return 1;
    case TYPE_MINISTRING: lua_pushlstring(L, variant->ministring.data, variant->ministring.length); return 1;
    case TYPE_POINTER: lua_pushlightuserdata(L, variant->pointer.value); return 1;
    case TYPE_OBJECT: _luax_pushtype(L, variant->object.type, variant->object.pointer); return 1;
    case TYPE_VECTOR: for (uint32_t i = 0; i < 3; i++) lua_pushnumber(L, variant->vector.data[i]); return 3;
    case TYPE_QUATERNION: for (uint32_t i = 0; i < 4; i++) lua_pushnumber(L, MAX(-1.f, variant->quaternion.data[i] / 32767.f)); return 4;
    case TYPE_TABLE:
      lua_newtable(L);
      for (size_t i = 0; i < variant->table.count; i++) {
        luax_pushvariant(L, &variant->table.pairs[2 * i + 0]);
        luax_pushvariant(L, &variant->table.pairs[2 * i + 1]);
        lua_settable(L, -3);
      }
      return 1;
    default: return 0;
  }
}

void luax_readcolor(lua_State* L, int index, float color[4]) {
  color[0] = color[1] = color[2] = color[3] = 1.f;

  if (lua_istable(L, index)) {
    if (luax_len(L, index) > 0) {
      for (int i = 1; i <= 4; i++) {
        lua_rawgeti(L, index, i);
      }
    } else {
      lua_getfield(L, index, "x");
      lua_getfield(L, index, "y");
      lua_getfield(L, index, "z");
      lua_getfield(L, index, "w");
    }
    color[0] = luax_checkfloat(L, -4);
    color[1] = luax_checkfloat(L, -3);
    color[2] = luax_checkfloat(L, -2);
    color[3] = luax_optfloat(L, -1, 1.);
    lua_pop(L, 4);
#ifdef LOVR_USE_LUAU
  } else if (lua_isvector(L, index)) {
    vec3_init(color, lua_tovector(L, index));
    color[3] = 1.f;
#endif
  } else if (lua_gettop(L) >= index + 2) {
    color[0] = luax_checkfloat(L, index);
    color[1] = luax_checkfloat(L, index + 1);
    color[2] = luax_checkfloat(L, index + 2);
    color[3] = luax_optfloat(L, index + 3, 1.);
  } else if (lua_gettop(L) <= index + 1) {
    uint32_t x = luaL_checkinteger(L, index);
    color[0] = ((x >> 16) & 0xff) / 255.f;
    color[1] = ((x >> 8) & 0xff) / 255.f;
    color[2] = ((x >> 0) & 0xff) / 255.f;
    color[3] = luax_optfloat(L, index + 1, 1.);
  }
}

// Like readcolor, but only consumes 1 argument (nil, hex, table, vec3, vec4), useful for table keys
void luax_optcolor(lua_State* L, int index, float color[4]) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
      color[0] = color[1] = color[2] = color[3] = 1.f;
      break;
    case LUA_TNUMBER: {
      uint32_t x = lua_tonumber(L, index);
      color[0] = ((x >> 16) & 0xff) / 255.f;
      color[1] = ((x >> 8) & 0xff) / 255.f;
      color[2] = ((x >> 0) & 0xff) / 255.f;
      color[3] = 1.f;
      break;
    }
    case LUA_TTABLE:
      index = index > 0 ? index : (index + lua_gettop(L) + 1);
      if (luax_len(L, index) > 0) {
        for (int i = 1; i <= 4; i++) {
          lua_rawgeti(L, index, i);
        }
      } else {
        lua_getfield(L, index, "x");
        lua_getfield(L, index, "y");
        lua_getfield(L, index, "z");
        lua_getfield(L, index, "w");
      }
      color[0] = luax_checkfloat(L, -4);
      color[1] = luax_checkfloat(L, -3);
      color[2] = luax_checkfloat(L, -2);
      color[3] = luax_optfloat(L, -1, 1.);
      lua_pop(L, 4);
      break;
#ifdef LOVR_USE_LUAU
    case LUA_TVECTOR: {
      vec3_init(color, lua_tovector(L, index));
      color[3] = 1.f;
      break;
    }
#endif
    default: luaL_error(L, "Expected nil, number, table, vec3, or vec4 for color value");
  }
}

int luax_readmesh(lua_State* L, int index, float** vertices, uint32_t* vertexCount, uint32_t** indices, uint32_t* indexCount) {
  if (lua_istable(L, index)) {
    lua_rawgeti(L, index, 1);
    bool nested = lua_type(L, -1) == LUA_TTABLE;
    lua_pop(L, 1);

    *vertexCount = luax_len(L, index) / (nested ? 1 : 3);
    luax_check(L, *vertexCount > 0, "Invalid mesh data: vertex count is zero");
    *vertices = lovrMalloc(sizeof(float) * *vertexCount * 3);

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

    if (indices) {
      luaL_checktype(L, index + 1, LUA_TTABLE);
      *indexCount = luax_len(L, index + 1);
      luax_check(L, *indexCount > 0, "Invalid mesh data: index count is zero");
      luax_check(L, *indexCount % 3 == 0, "Index count must be a multiple of 3");
      *indices = lovrMalloc(sizeof(uint32_t) * *indexCount);

      for (uint32_t i = 0; i < *indexCount; i++) {
        lua_rawgeti(L, index + 1, i + 1);
        uint32_t index = luaL_checkinteger(L, -1) - 1;
        luax_check(L, index < *vertexCount, "Invalid vertex index %d (expected [%d, %d])", index + 1, 1, *vertexCount);
        (*indices)[i] = index;
        lua_pop(L, 1);
      }
    }

    return index + (indices ? 2 : 1);
  }

  ModelData* modelData = luax_totype(L, index, ModelData);

  if (modelData) {
    lovrModelDataGetTriangles(modelData, vertices, indices, vertexCount, indexCount);
    return index + 1;
  }

#ifndef LOVR_DISABLE_GRAPHICS
  Mesh* mesh = luax_totype(L, index, Mesh);

  if (mesh) {
    luax_assert(L, lovrMeshGetTriangles(mesh, vertices, indices, vertexCount, indexCount));
    return index + 1;
  }

  luaL_argerror(L, index, "table, ModelData, or Mesh expected");
#else
  luaL_argerror(L, index, "table or ModelData expected");
#endif
  return 0;
}

int luax_readvec3(lua_State* L, int index, vec3 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 0.f;
      return index + 1;
    case LUA_TNUMBER:
      v[0] = luax_tofloat(L, index);
      v[1] = luax_optfloat(L, index + 1, v[0]);
      v[2] = luax_optfloat(L, index + 2, v[0]);
      return index + 3;
    case LUA_TTABLE:
      index = index > 0 ? index : (index + lua_gettop(L) + 1);
      if (luax_len(L, index) > 0) {
        lua_rawgeti(L, index, 1);
        lua_rawgeti(L, index, 2);
        lua_rawgeti(L, index, 3);
        v[0] = luax_tofloat(L, -3);
        v[1] = luax_tofloat(L, -2);
        v[2] = luax_tofloat(L, -1);
        lua_pop(L, 3);
      } else {
        lua_pushliteral(L, "x");
        lua_gettable(L, index);

        lua_pushliteral(L, "y");
        lua_gettable(L, index);

        lua_pushliteral(L, "z");
        lua_gettable(L, index);

        v[0] = luax_tofloat(L, -3);
        v[1] = luax_tofloat(L, -2);
        v[2] = luax_tofloat(L, -1);
        lua_pop(L, 3);
      }

      return index + 1;
#ifdef LOVR_USE_LUAU
    case LUA_TVECTOR:
      vec3_init(v, lua_tovector(L, index));
      return index + 1;
#endif
    default: return luax_typeerror(L, index, "number, table, or vector");
  }
}

int luax_readscale(lua_State* L, int index, vec3 v, int components, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 1.f;
      return index + 1;
    case LUA_TNUMBER:
      if (components == 1) {
        v[0] = v[1] = v[2] = luax_tofloat(L, index);
      } else if (components == -2) { // -2 is special and means "2 components: xy and z"
        v[0] = v[1] = luax_tofloat(L, index);
        v[2] = luax_optfloat(L, index + 1, 1.f);
        return index + 2;
      } else {
        v[0] = v[1] = v[2] = 1.f;
        for (int i = 0; i < components; i++) {
          v[i] = luax_optfloat(L, index + i, v[0]);
        }
      }
      return index + components;
    case LUA_TTABLE:
      index = index > 0 ? index : (index + lua_gettop(L) + 1);
      if (luax_len(L, index) > 0) {
        lua_rawgeti(L, index, 1);
        lua_rawgeti(L, index, 2);
        lua_rawgeti(L, index, 3);
        v[0] = luax_tofloat(L, -3);
        v[1] = luax_tofloat(L, -2);
        v[2] = luax_tofloat(L, -1);
        lua_pop(L, 3);
      } else {
        lua_pushliteral(L, "x");
        lua_gettable(L, index);

        lua_pushliteral(L, "y");
        lua_gettable(L, index);

        lua_pushliteral(L, "z");
        lua_gettable(L, index);

        v[0] = luax_tofloat(L, -3);
        v[1] = luax_tofloat(L, -2);
        v[2] = luax_tofloat(L, -1);
        lua_pop(L, 3);
      }
      return index + 1;
#ifdef LOVR_USE_LUAU
    case LUA_TVECTOR:
      vec3_init(v, lua_tovector(L, index));
      return index + 1;
#endif
    default: return luax_typeerror(L, index, expected ? expected : "nil, number, table, or vector");
  }
}

int luax_readquat(lua_State* L, int index, quat q, const char* expected) {
  float angle, ax, ay, az;
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      quat_identity(q);
      return index + 1;
    case LUA_TNUMBER:
      angle = luax_optfloat(L, index, 0.f);
      ax = luax_optfloat(L, index + 1, 0.f);
      ay = luax_optfloat(L, index + 2, 1.f);
      az = luax_optfloat(L, index + 3, 0.f);
      quat_fromAngleAxis(q, angle, ax, ay, az);
      return index + 4;
    case LUA_TTABLE:
      index = index > 0 ? index : (index + lua_gettop(L) + 1);
      if (luax_len(L, index) > 0) {
        lua_rawgeti(L, index, 1);
        lua_rawgeti(L, index, 2);
        lua_rawgeti(L, index, 3);
        lua_rawgeti(L, index, 4);
        q[0] = luax_tofloat(L, -4);
        q[1] = luax_tofloat(L, -3);
        q[2] = luax_tofloat(L, -2);
        q[3] = luax_tofloat(L, -1);
        lua_pop(L, 4);
      } else {
        lua_pushliteral(L, "x");
        lua_gettable(L, index);

        lua_pushliteral(L, "y");
        lua_gettable(L, index);

        lua_pushliteral(L, "z");
        lua_gettable(L, index);

        lua_pushliteral(L, "w");
        lua_gettable(L, index);

        q[0] = luax_tofloat(L, -4);
        q[1] = luax_tofloat(L, -3);
        q[2] = luax_tofloat(L, -2);
        q[3] = luax_tofloat(L, -1);
        lua_pop(L, 4);
      }
      return index + 1;
#ifdef LOVR_USE_LUAU
    case LUA_TQUATERNION: {
      const short* s = lua_toquaternion(L, index);
      q[0] = MAX(s[0] / 32767.f, -1.f);
      q[1] = MAX(s[1] / 32767.f, -1.f);
      q[2] = MAX(s[2] / 32767.f, -1.f);
      q[3] = MAX(s[3] / 32767.f, -1.f);
      quat_normalize(q);
      return index + 1;
    }
#endif
    default: return luax_typeerror(L, index, expected ? expected : "nil, number, table, or quaternion");
  }
}

int luax_readmat4(lua_State* L, int index, mat4 m, int scaleComponents) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      mat4_identity(m);
      return index + 1;
#ifdef LOVR_USE_LUAU
    case LUA_TVECTOR:
#endif
    case LUA_TNUMBER:
    case LUA_TTABLE:;
      float T[3], R[4], S[3];
      index = luax_readvec3(L, index, T, "number, table, vector, or Mat4");
      index = luax_readscale(L, index, S, scaleComponents, NULL);
      index = luax_readquat(L, index, R, NULL);
      mat4_fromPose(m, T, R);
      mat4_scale(m, S[0], S[1], S[2]);
      return index;
    default:;
      Mat4* matrix = luax_totype(L, index, Mat4);
      if (matrix) {
        mat4_init(m, lovrMat4GetData(matrix));
        return index + 1;
      }
      return luax_typeerror(L, index, "number, table, vector, or Mat4");
  }
}

void luax_pushvec3(lua_State* L, float v[3], bool tableArray) {
  if (tableArray) {
    lua_createtable(L, 3, 0);
    lua_pushnumber(L, v[0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, v[1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, v[2]);
    lua_rawseti(L, -2, 3);
  } else {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, v[0]);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v[1]);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v[2]);
    lua_setfield(L, -2, "z");
  }
}

bool luax_isquat(lua_State* L, int index) {
  if (lua_istable(L, index)) {
    int len = luax_len(L, index);
    if (len == 4) {
      return true;
    } else if (len == 0) {
      lua_pushstring(L, "w");
      lua_gettable(L, index);
      bool number = lua_type(L, -1) == LUA_TNUMBER;
      lua_pop(L, 1);
      return number;
    }
  }

#ifdef LOVR_USE_LUAU
  return lua_type(L, index) == LUA_TQUATERNION;
#endif

  return false;
}
