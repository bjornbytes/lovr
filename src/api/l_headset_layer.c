#include "api.h"

static int l_lovrLayerGetWidth(lua_State* L) {
  lua_pushnumber(L, 0.);
  return 1;
}

const luaL_Reg lovrLayer[] = {
  { "getWidth", l_lovrLayerGetWidth },
  { NULL, NULL }
};
