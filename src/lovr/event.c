#include "lovr/event.h"
#include "event/event.h"

const luaL_Reg lovrEvent[] = {
  { "poll", l_lovrEventPoll },
  { "quit", l_lovrEventQuit },
  { NULL, NULL }
};

int l_lovrEventInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);
  return 1;
}

int l_lovrEventPoll(lua_State* L) {
  lovrEventPoll();
  return 0;
}

int l_lovrEventQuit(lua_State* L) {
  int exitCode = luaL_optint(L, 1, 0);
  lovrEventQuit(exitCode);
  return 0;
}
