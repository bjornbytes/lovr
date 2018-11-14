#include "luax.h"
#include "platform.h"
#include "util.h"
#include "lib/sds/sds.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

static int luax_meta__tostring(lua_State* L) {
  lua_getfield(L, -1, "name");
  return 1;
}

static int luax_meta__gc(lua_State* L) {
  lovrRelease(*(Ref**) lua_touserdata(L, 1));
  return 0;
}

static int luax_module__gc(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrmodules");
  for (int i = lua_objlen(L, 2); i >= 1; i--) {
    lua_rawgeti(L, 2, i);
    luax_destructor destructor = (luax_destructor) lua_touserdata(L, -1);
    destructor();
    lua_pop(L, 1);
  }
  return 0;
}

// A version of print that uses lovrLog, for platforms that need it (Android)
int luax_print(lua_State* L) {
  sds str = sdsempty();
  int n = lua_gettop(L);  /* number of arguments */
  int i;

  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("print"));
    if (i>1) str = sdscat(str, "\t");
    str = sdscat(str, s);
    lua_pop(L, 1);  /* pop result */
  }
  lovrLog("%s", str);

  sdsfree(str);
  return 0;
}

void luax_atexit(lua_State* L, luax_destructor destructor) {
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

  int length = lua_objlen(L, -1);
  lua_pushlightuserdata(L, (void*) destructor);
  lua_rawseti(L, -2, length + 1);
  lua_pop(L, 1);
}

void luax_registerloader(lua_State* L, lua_CFunction loader, int index) {
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "insert");
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");
  lua_remove(L, -2);
  if (lua_istable(L, -1)) {
    lua_pushinteger(L, index);
    lua_pushcfunction(L, loader);
    lua_call(L, 3, 0);
  }
  lua_pop(L, 1);
}

void luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions) {

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

  // m.__gc = gc
  lua_pushcfunction(L, luax_meta__gc);
  lua_setfield(L, -2, "__gc");

  // m.name = name
  lua_pushstring(L, name);
  lua_setfield(L, -2, "name");

  // m.__tostring
  lua_pushcfunction(L, luax_meta__tostring);
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

void* _luax_totype(lua_State* L, int index, const char* type) {
  void** p = lua_touserdata(L, index);

  if (p) {
    Ref* object = *(Ref**) p;
    if (!strcmp(object->type, type)) {
      return object;
    }

    if (lua_getmetatable(L, index)) {
      lua_getfield(L, -1, "super");
      const char* super = lua_tostring(L, -1);
      lua_pop(L, 2);
      return (!super || strcmp(super, type)) ? NULL : object;
    }
  }

  return NULL;
}

void* _luax_checktype(lua_State* L, int index, const char* type) {
  void* object = _luax_totype(L, index, type);

  if (!object) {
    luaL_typerror(L, index, type);
  }

  return object;
}

// Registers the userdata on the top of the stack in the registry.
void luax_pushobject(lua_State* L, void* object) {
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
    lua_pushstring(L, "v");
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
  void** u = (void**) lua_newuserdata(L, sizeof(void**));
  luaL_getmetatable(L, ((Ref*) object)->type);
  lua_setmetatable(L, -2);
  lovrRetain(object);
  *u = object;

  // Write to registry and remove registry, leaving userdata on stack
  lua_pushlightuserdata(L, object);
  lua_pushvalue(L, -2);
  lua_settable(L, -4);
  lua_remove(L, -2);
}

void luax_vthrow(lua_State* L, const char* format, va_list args) {
  lua_pushvfstring(L, format, args);
  lua_error(L);
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
  lua_remove(L, -2); // Pop debug object
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

int luax_pushLovrHeadsetRenderError(lua_State *L) {
  lua_getglobal(L, "_lovrHeadsetRenderError"); // renderCallback failed
  bool haveRenderError = !lua_isnil(L, -1);
  if (haveRenderError) {
    lua_pushnil(L); // Now the error is on the stack remove it from globals
    lua_setglobal(L, "_lovrHeadsetRenderError");
  } else {
    lua_pop(L, 1); // pop stray nil
  }
  return haveRenderError;
}

static lua_State *luax_mainstate;

lua_State *luax_getmainstate() {
  return luax_mainstate;
}

void luax_setmainstate(lua_State *L) {
  luax_mainstate = L;
}
