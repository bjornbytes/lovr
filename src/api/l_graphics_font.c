#include "api.h"
#include "graphics/font.h"
#include "data/rasterizer.h"
#include "core/util.h"

static int l_lovrFontGetWidth(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  size_t length;
  const char* string = luaL_checklstring(L, 2, &length);
  float wrap = luax_optfloat(L, 3, 0.f);
  float width;
  float height;
  uint32_t lineCount;
  uint32_t glyphCount;
  lovrFontMeasure(font, string, length, wrap, &width, &height, &lineCount, &glyphCount);
  lua_pushnumber(L, width);
  lua_pushnumber(L, lineCount + 1);
  return 2;
}

static int l_lovrFontGetHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetHeight(font));
  return 1;
}

static int l_lovrFontGetAscent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetAscent(font));
  return 1;
}

static int l_lovrFontGetDescent(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetDescent(font));
  return 1;
}

static int l_lovrFontGetBaseline(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetBaseline(font));
  return 1;
}

static int l_lovrFontGetLineHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushinteger(L, lovrFontGetLineHeight(font));
  return 1;
}

static int l_lovrFontSetLineHeight(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  float lineHeight = luax_checkfloat(L, 2);
  lovrFontSetLineHeight(font, lineHeight);
  return 0;
}

static int l_lovrFontIsFlipEnabled(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushboolean(L, lovrFontIsFlipEnabled(font));
  return 1;
}

static int l_lovrFontSetFlipEnabled(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lovrFontSetFlipEnabled(font, lua_toboolean(L, 2));
  return 0;
}

static int l_lovrFontGetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lua_pushnumber(L, lovrFontGetPixelDensity(font));
  return 1;
}

static int l_lovrFontSetPixelDensity(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  if (lua_isnoneornil(L, 2)) {
    lovrFontSetPixelDensity(font, lovrRasterizerGetHeight(lovrFontGetRasterizer(font)));
  } else {
    float pixelDensity = luax_optfloat(L, 2, -1.f);
    lovrFontSetPixelDensity(font, pixelDensity);
  }
  return 0;
}

static int l_lovrFontGetRasterizer(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  luax_pushtype(L, Rasterizer, lovrFontGetRasterizer(font));
  return 1;
}

static int l_lovrFontHasGlyphs(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  Rasterizer* rasterizer = lovrFontGetRasterizer(font);
  bool hasGlyphs = true;
  if (lovrRasterizerIsDummy(rasterizer)) {
    for (int i = 2; i <= lua_gettop(L); i++) {
      if (lua_type(L, i) == LUA_TSTRING) {
        hasGlyphs &= lovrFontHasGlyphsCached(font, lua_tostring(L, i));
      } else {
        hasGlyphs &= lovrFontHasGlyphCached(font, luaL_checkinteger(L, i));
      }
    }
  } else {
    for (int i = 2; i <= lua_gettop(L); i++) {
      if (lua_type(L, i) == LUA_TSTRING) {
        hasGlyphs &= lovrRasterizerHasGlyphs(rasterizer, lua_tostring(L, i));
      } else {
        hasGlyphs &= lovrRasterizerHasGlyph(rasterizer, luaL_checkinteger(L, i));
      }
    }
  }
  lua_pushboolean(L, hasGlyphs);
  return 1;
}

static int l_lovrAddTextureGlyph(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  size_t length;
  const char* str = luaL_checklstring(L, 2, &length);

  unsigned int codepoint;
  const char* end = str + length;
  size_t bytes = utf8_decode(str, end, &codepoint);
  lovrAssert(bytes, "Argument 2 does not contain a Unicode character");

  uint32_t x = luaL_checknumber(L,3);
  uint32_t y = luaL_checknumber(L,4);
  uint32_t w = luaL_checknumber(L,5);
  uint32_t h = luaL_checknumber(L,6);
  uint32_t tw = luaL_checknumber(L,7);
  uint32_t th = luaL_checknumber(L,8);
  int32_t dx = luaL_checknumber(L,9);
  int32_t dy = luaL_checknumber(L,10);
  int32_t advance = luaL_checknumber(L,11);
  lovrFontAddTextureGlyph(font, codepoint, x, y, w, h, tw, th, dx, dy, advance);
  return 0;
}

const luaL_Reg lovrFont[] = {
  { "getWidth", l_lovrFontGetWidth },
  { "getHeight", l_lovrFontGetHeight },
  { "getAscent", l_lovrFontGetAscent },
  { "getDescent", l_lovrFontGetDescent },
  { "getBaseline", l_lovrFontGetBaseline },
  { "getLineHeight", l_lovrFontGetLineHeight },
  { "setLineHeight", l_lovrFontSetLineHeight },
  { "isFlipEnabled", l_lovrFontIsFlipEnabled },
  { "setFlipEnabled", l_lovrFontSetFlipEnabled },
  { "getPixelDensity", l_lovrFontGetPixelDensity },
  { "setPixelDensity", l_lovrFontSetPixelDensity },
  { "getRasterizer", l_lovrFontGetRasterizer},
  { "hasGlyphs", l_lovrFontHasGlyphs },
  { "addTextureGlyph", l_lovrAddTextureGlyph },
  { NULL, NULL }
};
