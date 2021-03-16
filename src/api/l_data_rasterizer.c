#include "api.h"
#include "data/rasterizer.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrRasterizerGetHeight(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetHeight(rasterizer));
  return 1;
}

static int l_lovrRasterizerGetAdvance(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetAdvance(rasterizer));
  return 1;
}

static int l_lovrRasterizerGetAscent(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetAscent(rasterizer));
  return 1;
}

static int l_lovrRasterizerGetDescent(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetDescent(rasterizer));
  return 1;
}

static int l_lovrRasterizerGetLineHeight(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetHeight(rasterizer) * 1.25f);
  return 1;
}

static int l_lovrRasterizerGetGlyphCount(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
  lua_pushinteger(L, lovrRasterizerGetGlyphCount(rasterizer));
  return 1;
}

static int l_lovrRasterizerHasGlyphs(lua_State* L) {
  Rasterizer* rasterizer = luax_checktype(L, 1, Rasterizer);
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

const luaL_Reg lovrRasterizer[] = {
  { "getHeight", l_lovrRasterizerGetHeight },
  { "getAdvance", l_lovrRasterizerGetAdvance },
  { "getAscent", l_lovrRasterizerGetAscent },
  { "getDescent", l_lovrRasterizerGetDescent },
  { "getLineHeight", l_lovrRasterizerGetLineHeight },
  { "getGlyphCount", l_lovrRasterizerGetGlyphCount },
  { "hasGlyphs", l_lovrRasterizerHasGlyphs },
  { NULL, NULL }
};
