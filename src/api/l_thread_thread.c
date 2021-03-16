#include "api.h"
#include "thread/thread.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrThreadStart(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  Variant arguments[MAX_THREAD_ARGUMENTS];
  uint32_t argumentCount = MIN(MAX_THREAD_ARGUMENTS, lua_gettop(L) - 1);
  for (uint32_t i = 0; i < argumentCount; i++) {
    luax_checkvariant(L, 2 + i, &arguments[i]);
  }
  lovrThreadStart(thread, arguments, argumentCount);
  return 0;
}

static int l_lovrThreadWait(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lovrThreadWait(thread);
  return 0;
}

static int l_lovrThreadGetError(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  const char* error = lovrThreadGetError(thread);
  if (error) {
    lua_pushstring(L, error);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrThreadIsRunning(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lua_pushboolean(L, thread->running);
  return 1;
}

const luaL_Reg lovrThread[] = {
  { "start", l_lovrThreadStart },
  { "wait", l_lovrThreadWait },
  { "getError", l_lovrThreadGetError },
  { "isRunning", l_lovrThreadIsRunning },
  { NULL, NULL }
};
