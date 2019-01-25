#include "api.h"
#include "graphics/font.h"

int l_lovrFontGetWidth(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  size_t length;
  const char* string = luaL_checklstring(L, 2, &length);
  float wrap = luaL_optnumber(L, 3, 0.f);
  float width;
  uint32_t lineCount;
  uint32_t glyphCount;
  lovrFontMeasure(font, string, length, wrap, &width, &lineCount, &glyphCount);
  lua_pushnumber(L, width);
  return 1;
}

int l_lovrFontGetHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetHeight(font));
  return 1;
}

int l_lovrFontGetAscent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetAscent(font));
  return 1;
}

int l_lovrFontGetDescent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetDescent(font));
  return 1;
}

int l_lovrFontGetBaseline(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetBaseline(font));
  return 1;
}

int l_lovrFontGetLineHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushinteger(L, lovrFontGetLineHeight(font));
  return 1;
}

int l_lovrFontSetLineHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float lineHeight = luaL_checknumber(L, 2);
  lovrFontSetLineHeight(font, lineHeight);
  return 0;
}

int l_lovrFontGetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetPixelDensity(font));
  return 1;
}

int l_lovrFontSetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  if (lua_isnoneornil(L, 2)) {
    lovrFontSetPixelDensity(font, lovrFontGetRasterizer(font)->height);
  } else {
    float pixelDensity = luaL_optnumber(L, 2, -1.f);
    lovrFontSetPixelDensity(font, pixelDensity);
  }
  return 0;
}

int l_lovrFontGetRasterizer(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  luax_pushobject(L, lovrFontGetRasterizer(font));
  return 1;
}

int l_lovrFontHasGlyphs(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  Rasterizer* rasterizer = lovrFontGetRasterizer(font);
  bool hasGlyphs = true;
  for (int i = 2; i <= lua_gettop(L); i++) {
    if (lua_type(L, i) == LUA_TSTRING) {
      hasGlyphs &= lovrRasterizerHasGlyphs(rasterizer, lua_tostring(L, i));
    } else {
      hasGlyphs &= lovrRasterizerHasGlyph(rasterizer, luaL_checkinteger(L, i));
    }
  }
  lua_pushboolean(L, hasGlyphs);
  return 1;
}

const luaL_Reg lovrFont[] = {
  { "getWidth", l_lovrFontGetWidth },
  { "getHeight", l_lovrFontGetHeight },
  { "getAscent", l_lovrFontGetAscent },
  { "getDescent", l_lovrFontGetDescent },
  { "getBaseline", l_lovrFontGetBaseline },
  { "getLineHeight", l_lovrFontGetLineHeight },
  { "setLineHeight", l_lovrFontSetLineHeight },
  { "getPixelDensity", l_lovrFontGetPixelDensity },
  { "setPixelDensity", l_lovrFontSetPixelDensity },
  { "getRasterizer", l_lovrFontGetRasterizer},
  { "hasGlyphs", l_lovrFontHasGlyphs },
  { NULL, NULL }
};
