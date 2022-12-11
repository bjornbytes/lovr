#include "api.h"
#include "thread/thread.h"
#include "event/event.h"
#include "util.h"
#include <math.h>

static void luax_checktimeout(lua_State* L, int index, double* timeout) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
      *timeout = NAN;
      break;
    case LUA_TBOOLEAN:
      *timeout = lua_toboolean(L, index) ? INFINITY : NAN;
      break;
    default:
      *timeout = luax_checkfloat(L, index);
      break;
  }
}

static int l_lovrChannelPush(lua_State* L) {
  Variant variant;
  double timeout;
  Channel* channel = luax_checktype(L, 1, Channel);
  luax_checkvariant(L, 2, &variant);
  luax_checktimeout(L, 3, &timeout);
  uint64_t id;
  bool read = lovrChannelPush(channel, &variant, timeout, &id);
  lua_pushnumber(L, id);
  lua_pushboolean(L, read);
  return 2;
}

static int l_lovrChannelPop(lua_State* L) {
  Variant variant;
  double timeout;
  Channel* channel = luax_checktype(L, 1, Channel);
  luax_checktimeout(L, 2, &timeout);
  if (lovrChannelPop(channel, &variant, timeout)) {
    luax_pushvariant(L, &variant);
    lovrVariantDestroy(&variant);
    return 1;
  }
  lua_pushnil(L);
  return 1;
}

static int l_lovrChannelPeek(lua_State* L) {
  Variant variant;
  Channel* channel = luax_checktype(L, 1, Channel);
  if (lovrChannelPeek(channel, &variant)) {
    luax_pushvariant(L, &variant);
    lua_pushboolean(L, true);
    return 2;
  }
  lua_pushnil(L);
  lua_pushboolean(L, false);
  return 2;
}

static int l_lovrChannelClear(lua_State* L) {
  Channel* channel = luax_checktype(L, 1, Channel);
  lovrChannelClear(channel);
  return 0;
}

static int l_lovrChannelGetCount(lua_State* L) {
  Channel* channel = luax_checktype(L, 1, Channel);
  lua_pushinteger(L, lovrChannelGetCount(channel));
  return 1;
}

static int l_lovrChannelHasRead(lua_State* L) {
  Channel* channel = luax_checktype(L, 1, Channel);
  uint64_t id = luaL_checkinteger(L, 2);
  lua_pushboolean(L, lovrChannelHasRead(channel, id));
  return 1;
}

const luaL_Reg lovrChannel[] = {
  { "push", l_lovrChannelPush },
  { "pop", l_lovrChannelPop },
  { "peek", l_lovrChannelPeek },
  { "clear", l_lovrChannelClear },
  { "getCount", l_lovrChannelGetCount },
  { "hasRead", l_lovrChannelHasRead },
  { NULL, NULL }
};
