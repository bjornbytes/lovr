#include "api.h"

int l_lovrThreadInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrThreadModule);
  return 1;
}

const luaL_Reg lovrThreadModule[] = {
  { NULL, NULL }
};
