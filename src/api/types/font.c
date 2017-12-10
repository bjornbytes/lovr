#include "api.h"
#include "graphics/font.h"

int l_lovrFontGetWidth(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  const char* string = luaL_checkstring(L, 2);
  float wrap = luaL_optnumber(L, 3, 0);
  lua_pushnumber(L, lovrFontGetWidth(font, string, wrap));
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
  float pixelDensity = luaL_optnumber(L, 2, -1);
  lovrFontSetPixelDensity(font, pixelDensity);
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
  { "getPixelDensity", l_lovrFontGetPixelDensity },
  { "setPixelDensity", l_lovrFontSetPixelDensity },
  { NULL, NULL }
};
