#include "api.h"
#include "graphics/graphics.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrGraphicsInit(lua_State* L) {
  bool debug = false;

  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrGraphicsInit(debug)) {
    luax_atexit(L, lovrGraphicsDestroy);
  }

  return 0;
}

static const luaL_Reg lovrGraphics[] = {
  { "init", l_lovrGraphicsInit },
  { NULL, NULL }
};

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  return 1;
}
