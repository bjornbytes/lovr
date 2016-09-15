#include "headset.h"
#include "../headset/headset.h"

void renderHelper(int eyeIndex, void* userdata) {
  lua_State* L = (lua_State*)userdata;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, eyeIndex);
  lua_call(L, 1, 0);
}

const luaL_Reg lovrHeadset[] = {
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "renderTo", l_lovrHeadsetRenderTo },
  { NULL, NULL }
};

int l_lovrHeadsetInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);
  lovrHeadsetInit();
  return 1;
}

int l_lovrHeadsetGetDisplayWidth(lua_State* L) {
  lua_pushinteger(L, lovrHeadsetGetDisplayWidth());
  return 1;
}

int l_lovrHeadsetGetDisplayHeight(lua_State* L) {
  lua_pushinteger(L, lovrHeadsetGetDisplayHeight());
  return 1;
}

int l_lovrHeadsetGetDisplayDimensions(lua_State* L) {
  lua_pushinteger(L, lovrHeadsetGetDisplayWidth());
  lua_pushinteger(L, lovrHeadsetGetDisplayHeight());
  return 2;
}

int l_lovrHeadsetRenderTo(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lovrHeadsetRenderTo(renderHelper, L);
  return 0;
}
