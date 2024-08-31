#include "api.h"
#include "data/rasterizer.h"
#include "data/image.h"
#include "util.h"
#include <math.h>

uint32_t luax_checkcodepoint(lua_State* L, int index) {
  size_t length;
  const char* str;
  uint32_t codepoint;
  switch (lua_type(L, index)) {
    case LUA_TSTRING:
      str = lua_tolstring(L, index, &length);
      return utf8_decode(str, str + length, &codepoint) ? codepoint : 0;
    case LUA_TNUMBER:
      return luax_checku32(L, index);
    default: return luax_typeerror(L, index, "string or number"), 0;
  }
}

static int l_lovrRasterizerGetFontSize(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float size = lovrRasterizerGetFontSize(rasterizer);
  lua_pushnumber(L, size);
  return 1;
}

static int l_lovrRasterizerGetGlyphCount(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  size_t count = lovrRasterizerGetGlyphCount(rasterizer);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrRasterizerHasGlyphs(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  for (int i = 2; i <= lua_gettop(L); i++) {
    size_t length;
    const char* str;
    switch (lua_type(L, i)) {
      case LUA_TSTRING:
        str = lua_tolstring(L, i, &length);
        if (!lovrRasterizerHasGlyphs(rasterizer, str, length)) {
          lua_pushboolean(L, false);
          return 1;
        }
        break;
      case LUA_TNUMBER:
        if (!lovrRasterizerHasGlyph(rasterizer, luax_checku32(L, i))) {
          lua_pushboolean(L, false);
          return 1;
        }
        break;
      default: return luax_typeerror(L, i, "string or number");
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrRasterizerGetAscent(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float ascent = lovrRasterizerGetAscent(rasterizer);
  lua_pushnumber(L, ascent);
  return 1;
}

static int l_lovrRasterizerGetDescent(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float descent = lovrRasterizerGetDescent(rasterizer);
  lua_pushnumber(L, descent);
  return 1;
}

static int l_lovrRasterizerGetLeading(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float leading = lovrRasterizerGetLeading(rasterizer);
  lua_pushnumber(L, leading);
  return 1;
}

static int l_lovrRasterizerGetAdvance(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  uint32_t codepoint = luax_checkcodepoint(L, 2);
  float advance = lovrRasterizerGetAdvance(rasterizer, codepoint);
  lua_pushnumber(L, advance);
  return 1;
}

static int l_lovrRasterizerGetBearing(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  uint32_t codepoint = luax_checkcodepoint(L, 2);
  float bearing = lovrRasterizerGetBearing(rasterizer, codepoint);
  lua_pushnumber(L, bearing);
  return 1;
}

static int l_lovrRasterizerGetKerning(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  uint32_t first = luax_checkcodepoint(L, 2);
  uint32_t second = luax_checkcodepoint(L, 3);
  float kerning = lovrRasterizerGetKerning(rasterizer, first, second);
  lua_pushnumber(L, kerning);
  return 1;
}

static void luax_getboundingbox(lua_State* L, int index, Rasterizer* rasterizer, float box[4]) {
  if (lua_isnoneornil(L, index)) {
    lovrRasterizerGetBoundingBox(rasterizer, box);
  } else {
    uint32_t codepoint = luax_checkcodepoint(L, index);
    lovrRasterizerGetGlyphBoundingBox(rasterizer, codepoint, box);
  }
}

static int l_lovrRasterizerGetWidth(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float box[4];
  luax_getboundingbox(L, 2, rasterizer, box);
  lua_pushnumber(L, box[2] - box[0]);
  return 1;
}

static int l_lovrRasterizerGetHeight(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float box[4];
  luax_getboundingbox(L, 2, rasterizer, box);
  lua_pushnumber(L, box[3] - box[1]);
  return 1;
}

static int l_lovrRasterizerGetDimensions(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float box[4];
  luax_getboundingbox(L, 2, rasterizer, box);
  lua_pushnumber(L, box[2] - box[0]);
  lua_pushnumber(L, box[3] - box[1]);
  return 2;
}

static int l_lovrRasterizerGetBoundingBox(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  float box[4];
  luax_getboundingbox(L, 2, rasterizer, box);
  lua_pushnumber(L, box[0]);
  lua_pushnumber(L, box[1]);
  lua_pushnumber(L, box[2]);
  lua_pushnumber(L, box[3]);
  return 4;
}

static void onCurve2D(void* context, uint32_t degree, float* points) {
  lua_State* L = context;
  uint32_t count = (degree + 1) * 2;
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    lua_pushnumber(L, points[i]);
    lua_rawseti(L, -2, i + 1);
  }
  lua_rawseti(L, -2, luax_len(L, -2) + 1);
}

static void onCurve3D(void* context, uint32_t degree, float* points) {
  lua_State* L = context;
  lua_createtable(L, (degree + 1) * 3, 0);
  for (uint32_t i = 0; i < degree + 1; i++) {
    lua_pushnumber(L, points[2 * i + 0]);
    lua_rawseti(L, -2, 3 * i + 1);
    lua_pushnumber(L, points[2 * i + 1]);
    lua_rawseti(L, -2, 3 * i + 2);
    lua_pushnumber(L, 0.);
    lua_rawseti(L, -2, 3 * i + 3);
  }
  lua_rawseti(L, -2, luax_len(L, -2) + 1);
}

static int l_lovrRasterizerGetCurves(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  uint32_t codepoint = luax_checkcodepoint(L, 2);
  bool three = lua_toboolean(L, 3);
  lua_newtable(L);
  lovrRasterizerGetCurves(rasterizer, codepoint, three ? onCurve3D : onCurve2D, L);
  return 1;
}

static int l_lovrRasterizerNewImage(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  uint32_t codepoint = luax_checkcodepoint(L, 2);
  double spread = lovrRasterizerGetType(rasterizer) == RASTERIZER_TTF ? luaL_optnumber(L, 3, 4.) : 0.;
  uint32_t padding = (uint32_t) ceil(spread / 2.);
  float box[4];
  lovrRasterizerGetGlyphBoundingBox(rasterizer, codepoint, box);
  uint32_t width = 2 * padding + (uint32_t) ceilf(box[2] - box[0]);
  uint32_t height = 2 * padding + (uint32_t) ceilf(box[3] - box[1]);
  Image* image = lovrImageCreateRaw(width, height, FORMAT_RGBA32F, false);
  luax_assert(L, image);
  void* pixels = lovrImageGetLayerData(image, 0, 0);
  lovrRasterizerGetPixels(rasterizer, codepoint, pixels, width, height, spread);
  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  return 1;
}

const luaL_Reg lovrRasterizer[] = {
  { "getFontSize", l_lovrRasterizerGetFontSize },
  { "getGlyphCount", l_lovrRasterizerGetGlyphCount },
  { "hasGlyphs", l_lovrRasterizerHasGlyphs },
  { "getAscent", l_lovrRasterizerGetAscent },
  { "getDescent", l_lovrRasterizerGetDescent },
  { "getLeading", l_lovrRasterizerGetLeading },
  { "getAdvance", l_lovrRasterizerGetAdvance },
  { "getBearing", l_lovrRasterizerGetBearing },
  { "getKerning", l_lovrRasterizerGetKerning },
  { "getWidth", l_lovrRasterizerGetWidth },
  { "getHeight", l_lovrRasterizerGetHeight },
  { "getDimensions", l_lovrRasterizerGetDimensions },
  { "getBoundingBox", l_lovrRasterizerGetBoundingBox },
  { "getCurves", l_lovrRasterizerGetCurves },
  { "newImage", l_lovrRasterizerNewImage },
  { NULL, NULL }
};
