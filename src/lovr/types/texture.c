#include "lovr/types/texture.h"
#include "lovr/graphics.h"
#include "util.h"

const luaL_Reg lovrTexture[] = {
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getFilter", l_lovrTextureGetFilter },
  { "getHeight", l_lovrTextureGetHeight },
  { "getWidth", l_lovrTextureGetWidth },
  { "getWrap", l_lovrTextureGetWrap },
  { "setFilter", l_lovrTextureSetFilter },
  { "setWrap", l_lovrTextureSetWrap },
  { NULL, NULL }
};

int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 2;
}

int l_lovrTextureGetFilter(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  FilterMode min, mag;
  lovrTextureGetFilter(texture, &min, &mag);
  lua_pushstring(L, map_int_find(&FilterModes, min));
  lua_pushstring(L, map_int_find(&FilterModes, mag));
  return 2;
}

int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 1;
}

int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  return 1;
}

int l_lovrTextureGetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  WrapMode horizontal, vertical;
  lovrTextureGetWrap(texture, &horizontal, &vertical);
  lua_pushstring(L, map_int_find(&WrapModes, horizontal));
  lua_pushstring(L, map_int_find(&WrapModes, vertical));
  return 2;
}

int l_lovrTextureSetFilter(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  FilterMode* min = (FilterMode*) luax_checkenum(L, 2, &FilterModes, "filter mode");
  FilterMode* mag = (FilterMode*) luax_optenum(L, 3, luaL_checkstring(L, 2), &FilterModes, "filter mode");
  lovrTextureSetFilter(texture, *min, *mag);
  return 0;
}

int l_lovrTextureSetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  WrapMode* horizontal = (WrapMode*) luax_checkenum(L, 2, &WrapModes, "wrap mode");
  WrapMode* vertical = (WrapMode*) luax_optenum(L, 3, luaL_checkstring(L, 2), &WrapModes, "wrap mode");
  lovrTextureSetWrap(texture, *horizontal, *vertical);
  return 0;
}
