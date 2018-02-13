#include "api.h"
#include "thread/thread.h"

int l_lovrThreadStart(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lovrThreadStart(thread);
  return 0;
}

int l_lovrThreadWait(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lovrThreadWait(thread);
  return 0;
}

int l_lovrThreadGetError(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  const char* error = lovrThreadGetError(thread);
  if (error) {
    lua_pushstring(L, error);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrThreadIsRunning(lua_State* L) {
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
