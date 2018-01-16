#include "api.h"
#include "data/data.h"

int l_lovrDataInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrData);
  return 1;
}

const luaL_Reg lovrData[] = {
  { NULL, NULL }
};
