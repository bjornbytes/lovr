#include "api/types/texture.h"
#include "api/graphics.h"
#include "graphics/graphics.h"

const luaL_Reg lovrTexture[] = {
  { "bind", l_lovrTextureBind },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getFilter", l_lovrTextureGetFilter },
  { "getHeight", l_lovrTextureGetHeight },
  { "getWidth", l_lovrTextureGetWidth },
  { "getWrap", l_lovrTextureGetWrap },
  { "renderTo", l_lovrTextureRenderTo },
  { "setFilter", l_lovrTextureSetFilter },
  { "setWrap", l_lovrTextureSetWrap },
  { NULL, NULL }
};

int l_lovrTextureBind(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lovrGraphicsBindTexture(texture);
  return 0;
}

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
  luax_pushenum(L, &FilterModes, min);
  luax_pushenum(L, &FilterModes, mag);
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
  luax_pushenum(L, &WrapModes, horizontal);
  luax_pushenum(L, &WrapModes, vertical);
  return 2;
}

int l_lovrTextureRenderTo(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lovrGraphicsPushCanvas();
  lovrTextureBindFramebuffer(texture);
  lua_settop(L, 2);
  lua_call(L, 0, 0);
  lovrTextureResolveMSAA(texture);
  lovrGraphicsPopCanvas();
  return 0;
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
