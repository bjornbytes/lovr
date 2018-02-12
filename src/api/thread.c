#include "api.h"
#include "thread/thread.h"

int l_lovrThreadInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrThreadModule);
  luax_registertype(L, "Thread", lovrThread);
  return 1;
}

int l_lovrThreadNewThread(lua_State* L) {
  const char* body = luaL_checkstring(L, 1);
  Thread* thread = lovrThreadCreate(body);
  luax_pushtype(L, Thread, thread);
  lovrRelease(&thread->ref);
  return 1;
}

const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { NULL, NULL }
};
