#include "api.h"
#include "thread/channel.h"

static void luax_checkvariant(lua_State* L, int index, Variant* variant) {
  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
      variant->type = TYPE_NIL;
      break;

    case LUA_TBOOLEAN:
      variant->type = TYPE_BOOLEAN;
      variant->value.boolean = lua_toboolean(L, index);
      break;

    case LUA_TNUMBER:
      variant->type = TYPE_NUMBER;
      variant->value.number = lua_tonumber(L, index);
      break;

    case LUA_TSTRING:
      variant->type = TYPE_STRING;
      size_t length;
      const char* string = lua_tolstring(L, index, &length);
      variant->value.string = malloc(length + 1);
      strcpy(variant->value.string, string);
      break;

    case LUA_TUSERDATA:
      lua_getmetatable(L, index);
      lua_getfield(L, -1, "name");
      variant->meta = luaL_checkstring(L, -1);
      lua_pop(L, 2);
      variant->type = TYPE_OBJECT;
      variant->value.ref = lua_touserdata(L, index);
      lovrRetain(variant->value.ref);
      break;

    default:
      lovrThrow("Bad type for Channel:push: %s", lua_typename(L, type));
      return;
  }
}

static int luax_pushvariant(lua_State* L, Variant* variant) {
  switch (variant->type) {
    case TYPE_NIL: lua_pushnil(L); break;
    case TYPE_BOOLEAN: lua_pushboolean(L, variant->value.boolean); break;
    case TYPE_NUMBER: lua_pushnumber(L, variant->value.number); break;
    case TYPE_STRING: lua_pushstring(L, variant->value.string); break;
    case TYPE_OBJECT:
      if (!luax_getobject(L, variant->value.ref)) {
        Ref** u = (Ref**) lua_newuserdata(L, sizeof(Ref**));
        luax_registerobject(L, variant->value.ref);
        luaL_getmetatable(L, variant->meta);
        lua_setmetatable(L, -2);
        *u = variant->value.ref;
        lovrRelease(variant->value.ref);
      }
      break;
  }

  if (variant->type == TYPE_STRING) {
    free(variant->value.string);
  }

  return 1;
}

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
      *timeout = luaL_checknumber(L, index);
      break;
  }
}

int l_lovrChannelPush(lua_State* L) {
  Variant variant;
  double timeout;
  Channel* channel = luax_checktype(L, 1, Channel);
  luax_checkvariant(L, 2, &variant);
  luax_checktimeout(L, 3, &timeout);
  uint64_t id;
  bool read = lovrChannelPush(channel, variant, timeout, &id);
  lua_pushnumber(L, id);
  lua_pushboolean(L, read);
  return 2;
}

int l_lovrChannelPop(lua_State* L) {
  Variant variant;
  double timeout;
  Channel* channel = luax_checktype(L, 1, Channel);
  luax_checktimeout(L, 2, &timeout);
  if (lovrChannelPop(channel, &variant, timeout)) {
    return luax_pushvariant(L, &variant);
  }
  lua_pushnil(L);
  return 1;
}

int l_lovrChannelPeek(lua_State* L) {
  Variant variant;
  Channel* channel = luax_checktype(L, 1, Channel);
  if (lovrChannelPeek(channel, &variant)) {
    return luax_pushvariant(L, &variant);
  }
  lua_pushnil(L);
  return 1;
}

int l_lovrChannelClear(lua_State* L) {
  Channel* channel = luax_checktype(L, 1, Channel);
  lovrChannelClear(channel);
  return 0;
}

int l_lovrChannelGetCount(lua_State* L) {
  Channel* channel = luax_checktype(L, 1, Channel);
  lua_pushinteger(L, lovrChannelGetCount(channel));
  return 1;
}

int l_lovrChannelHasRead(lua_State* L) {
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
