#include "api.h"
#include "lovr.h"
#include "resources/logo.png.h"

static int l_lovrGetOS(lua_State* L) {
  const char* os = lovrGetOS();
  if (os) {
    lua_pushstring(L, os);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrGetVersion(lua_State* L) {
  int major, minor, patch;
  lovrGetVersion(&major, &minor, &patch);
  lua_pushinteger(L, major);
  lua_pushinteger(L, minor);
  lua_pushinteger(L, patch);
  return 3;
}

static const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getOS", l_lovrGetOS },
  { "getVersion", l_lovrGetVersion },
  { NULL, NULL }
};

int luaopen_lovr(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovr);
  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");
  return 1;
}
