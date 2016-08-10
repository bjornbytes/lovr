#include "timer.h"
#include "../timer/timer.h"

const luaL_Reg lovrTimer[] = {
  { "step", l_lovrTimerStep },
  { NULL, NULL }
};

int l_lovrTimerInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrTimer);
  return 1;
}

int l_lovrTimerStep(lua_State* L) {
  lua_pushnumber(L, lovrTimerStep());
  return 1;
}
