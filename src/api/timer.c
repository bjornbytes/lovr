#include "api/lovr.h"
#include "timer/timer.h"

int l_lovrTimerInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrTimer);
  lovrTimerInit();
  return 1;
}

int l_lovrTimerGetDelta(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetDelta());
  return 1;
}

int l_lovrTimerGetAverageDelta(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetAverageDelta());
  return 1;
}

int l_lovrTimerGetFPS(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetFPS());
  return 1;
}

int l_lovrTimerGetTime(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetTime());
  return 1;
}

int l_lovrTimerStep(lua_State* L) {
  lua_pushnumber(L, lovrTimerStep());
  return 1;
}

int l_lovrTimerSleep(lua_State* L) {
  double duration = luaL_checknumber(L, 1);
  lovrTimerSleep(duration);
  return 0;
}

const luaL_Reg lovrTimer[] = {
  { "getDelta", l_lovrTimerGetDelta },
  { "getAverageDelta", l_lovrTimerGetAverageDelta },
  { "getFPS", l_lovrTimerGetFPS },
  { "getTime", l_lovrTimerGetTime },
  { "step", l_lovrTimerStep },
  { "sleep", l_lovrTimerSleep },
  { NULL, NULL }
};
