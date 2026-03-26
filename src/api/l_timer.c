#include "api.h"
#include "timer/timer.h"

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

static bool luax_polltime(void** context) {
  double timeout = ((union { double f64; void* p; }) { .p = *context }).f64;
  return lovrTimerGetTime() >= timeout;
}

static bool luax_waittime(void** context) {
  double timeout = ((union { double f64; void* p; }) { .p = *context }).f64;
  lovrTimerSleep(timeout - lovrTimerGetTime());
  return true;
}

static int l_lovrTimerSleep(lua_State* L) {
  double duration = luaL_checknumber(L, 1);
  if (luax_getthreaddata(L)) {
    void* timeout = ((union { double f64; void* p; }) { .f64 = lovrTimerGetTime() + duration }).p;
    return luax_yieldpoll(L, luax_polltime, luax_waittime, NULL, timeout);
  } else {
    lovrTimerSleep(duration);
    return 0;
  }
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
  lovrTimerInit();
  luax_atexit(L, lovrTimerDestroy);
  return 1;
}
