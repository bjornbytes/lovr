#include "api.h"
#include "timer/timer.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrTimerGetDelta(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetDelta());
  return 1;
}

static int l_lovrTimerGetAverageDelta(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetAverageDelta());
  return 1;
}

static int l_lovrTimerGetFPS(lua_State* L) {
  lua_pushinteger(L, lovrTimerGetFPS());
  return 1;
}

static int l_lovrTimerGetTime(lua_State* L) {
  lua_pushnumber(L, lovrTimerGetTime());
  return 1;
}

static int l_lovrTimerStep(lua_State* L) {
  lua_pushnumber(L, lovrTimerStep());
  return 1;
}

static int l_lovrTimerSleep(lua_State* L) {
  double duration = luaL_checknumber(L, 1);
  lovrTimerSleep(duration);
  return 0;
}

static const luaL_Reg lovrTimer[] = {
  { "getDelta", l_lovrTimerGetDelta },
  { "getAverageDelta", l_lovrTimerGetAverageDelta },
  { "getFPS", l_lovrTimerGetFPS },
  { "getTime", l_lovrTimerGetTime },
  { "step", l_lovrTimerStep },
  { "sleep", l_lovrTimerSleep },
  { NULL, NULL }
};

int luaopen_lovr_timer(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrTimer);
  if (lovrTimerInit()) {
    luax_atexit(L, lovrTimerDestroy);
  }
  return 1;
}
