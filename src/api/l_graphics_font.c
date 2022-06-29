#include "api.h"
#include "graphics/graphics.h"
#include "data/rasterizer.h"
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

static int l_lovrFontGetLineSpacing(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float spacing = lovrFontGetLineSpacing(font);
  lua_pushnumber(L, spacing);
  return 1;
}

static int l_lovrFontSetLineSpacing(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float spacing = luax_optfloat(L, 2, 1.f);
  lovrFontSetLineSpacing(font, spacing);
  return 0;
}

static int l_lovrFontGetAscent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  const FontInfo* info = lovrFontGetInfo(font);
  float density = lovrFontGetPixelDensity(font);
  float ascent = lovrRasterizerGetAscent(info->rasterizer);
  lua_pushnumber(L, ascent / density);
  return 1;
}

static int l_lovrFontGetDescent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  const FontInfo* info = lovrFontGetInfo(font);
  float density = lovrFontGetPixelDensity(font);
  float descent = lovrRasterizerGetDescent(info->rasterizer);
  lua_pushnumber(L, descent / density);
  return 1;
}

static int l_lovrFontGetHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  const FontInfo* info = lovrFontGetInfo(font);
  float density = lovrFontGetPixelDensity(font);
  float height = lovrRasterizerGetLeading(info->rasterizer);
  lua_pushnumber(L, height / density);
  return 1;
}

const luaL_Reg lovrFont[] = {
  { "getRasterizer", l_lovrFontGetRasterizer },
  { "getPixelDensity", l_lovrFontGetPixelDensity },
  { "setPixelDensity", l_lovrFontSetPixelDensity },
  { "getLineSpacing", l_lovrFontGetLineSpacing },
  { "setLineSpacing", l_lovrFontSetLineSpacing },
  { "getAscent", l_lovrFontGetAscent },
  { "getDescent", l_lovrFontGetDescent },
  { "getHeight", l_lovrFontGetHeight },
  { NULL, NULL }
};
