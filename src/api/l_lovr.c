#include "api.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrGetVersion(lua_State* L) {
  lua_pushinteger(L, LOVR_VERSION_MAJOR);
  lua_pushinteger(L, LOVR_VERSION_MINOR);
  lua_pushinteger(L, LOVR_VERSION_PATCH);
  return 3;
}

static const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getVersion", l_lovrGetVersion },
  { NULL, NULL }
};

int luaopen_lovr(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovr);
  return 1;
}
