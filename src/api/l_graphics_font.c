#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrFontGetRasterizer(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  const FontInfo* info = lovrFontGetInfo(font);
  luax_pushtype(L, Rasterizer, info->rasterizer);
  return 1;
}

static int l_lovrFontGetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float density = lovrFontGetPixelDensity(font);
  lua_pushnumber(L, density);
  return 1;
}

static int l_lovrFontSetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float pixelDensity = luax_optfloat(L, 2, 0.f);
  lovrFontSetPixelDensity(font, pixelDensity);
  return 0;
}

const luaL_Reg lovrFont[] = {
  { "getRasterizer", l_lovrFontGetRasterizer },
  { "getPixelDensity", l_lovrFontGetPixelDensity },
  { "setPixelDensity", l_lovrFontSetPixelDensity },
  { NULL, NULL }
};
