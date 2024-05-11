#include "api.h"
#include "graphics/graphics.h"
#include "data/rasterizer.h"
#include "util.h"
#include <stdlib.h>

ColoredString* luax_checkcoloredstrings(lua_State* L, int index, uint32_t* count, ColoredString* stack) {
  if (lua_istable(L, index)) {
    *count = luax_len(L, index) / 2;
    ColoredString* strings = malloc(*count * sizeof(*strings));
    lovrAssert(strings, "Out of memory");
    for (uint32_t i = 0; i < *count; i++) {
      lua_rawgeti(L, index, i * 2 + 1);
      lua_rawgeti(L, index, i * 2 + 2);
      luax_optcolor(L, -2, strings[i].color);
      lovrCheck(lua_isstring(L, -1), "Expected a string to print");
      strings[i].string = luaL_checklstring(L, -1, &strings[i].length);
      lua_pop(L, 2);
    }
    return strings;
  } else {
    stack->string = luaL_checklstring(L, index, &stack->length);
    stack->color[0] = stack->color[1] = stack->color[2] = stack->color[3] = 1.f;
    return *count = 1, stack;
  }
}

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
  Rasterizer* rasterizer = lovrFontGetInfo(font)->rasterizer;
  float pixelDensity = luax_optfloat(L, 2, lovrRasterizerGetLeading(rasterizer));
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

static int l_lovrFontGetKerning(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  uint32_t first = luax_checkcodepoint(L, 2);
  uint32_t second = luax_checkcodepoint(L, 3);
  float kerning = lovrFontGetKerning(font, first, second);
  float density = lovrFontGetPixelDensity(font);
  lua_pushnumber(L, kerning / density);
  return 1;
}

static int l_lovrFontGetWidth(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  uint32_t count;
  ColoredString stack;
  ColoredString* strings = luax_checkcoloredstrings(L, 2, &count, &stack);
  float width = lovrFontGetWidth(font, strings, count);
  lua_pushnumber(L, width);
  return 1;
}

static void online(void* context, const char* string, size_t length) {
  lua_State* L = context;
  int index = luax_len(L, -1) + 1;
  lua_pushlstring(L, string, length);
  lua_rawseti(L, -2, index);
}

static int l_lovrFontGetLines(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  uint32_t count;
  ColoredString stack;
  ColoredString* strings = luax_checkcoloredstrings(L, 2, &count, &stack);
  float wrap = luax_checkfloat(L, 3);
  lua_newtable(L);
  lovrFontGetLines(font, strings, count, wrap, online, L);
  if (strings != &stack) free(strings);
  return 1;
}

static int l_lovrFontGetVertices(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  uint32_t count;
  ColoredString stack;
  ColoredString* strings = luax_checkcoloredstrings(L, 2, &count, &stack);
  float wrap = luax_optfloat(L, 3, 0.f);
  HorizontalAlign halign = luax_checkenum(L, 4, HorizontalAlign, "center");
  VerticalAlign valign = luax_checkenum(L, 5, VerticalAlign, "middle");
  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }
  GlyphVertex* vertices = malloc(totalLength * 4 * sizeof(GlyphVertex));
  lovrAssert(vertices, "Out of memory");
  uint32_t glyphCount, lineCount;
  Material* material;
  lovrFontGetVertices(font, strings, count, wrap, halign, valign, vertices, &glyphCount, &lineCount, &material, false);
  int vertexCount = glyphCount * 4;
  lua_createtable(L, vertexCount, 0);
  for (int i = 0; i < vertexCount; i++) {
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, vertices[i].position.x);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, vertices[i].position.y);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, vertices[i].uv.u / 65535.f);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, vertices[i].uv.v / 65535.f);
    lua_rawseti(L, -2, 4);
    lua_rawseti(L, -2, i + 1);
  }
  luax_pushtype(L, Material, material);
  if (strings != &stack) free(strings);
  free(vertices);
  return 2;
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
  { "getKerning", l_lovrFontGetKerning },
  { "getWidth", l_lovrFontGetWidth },
  { "getLines", l_lovrFontGetLines },
  { "getVertices", l_lovrFontGetVertices },
  { NULL, NULL }
};
