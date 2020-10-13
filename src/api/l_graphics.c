#include "api.h"
#include "graphics/graphics.h"
#include <lua.h>
#include <lauxlib.h>

static const luaL_Reg lovrGraphics[] = {
  { NULL, NULL }
};

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  lovrGraphicsInit();
  return 1;
}
