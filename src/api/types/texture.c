#include "api/lovr.h"
#include "graphics/graphics.h"

int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, texture->width);
  lua_pushnumber(L, texture->height);
  return 2;
}

int l_lovrTextureGetFilter(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureFilter filter = lovrTextureGetFilter(texture);
  luax_pushenum(L, &FilterModes, filter.mode);
  if (filter.mode == FILTER_ANISOTROPIC) {
    lua_pushnumber(L, filter.anisotropy);
    return 2;
  }
  return 1;
}

int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, texture->height);
  return 1;
}

int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, texture->width);
  return 1;
}

int l_lovrTextureGetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureWrap wrap = lovrTextureGetWrap(texture);
  luax_pushenum(L, &WrapModes, wrap.s);
  luax_pushenum(L, &WrapModes, wrap.t);
  if (texture->type == TEXTURE_CUBE) {
    luax_pushenum(L, &WrapModes, wrap.r);
    return 3;
  }
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
  FilterMode mode = *(FilterMode*) luax_checkenum(L, 2, &FilterModes, "filter mode");
  float anisotropy = luaL_optnumber(L, 3, 1.);
  TextureFilter filter = { .mode = mode, .anisotropy = anisotropy };
  lovrTextureSetFilter(texture, filter);
  return 0;
}

int l_lovrTextureSetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureWrap wrap;
  wrap.s = *(WrapMode*) luax_checkenum(L, 2, &WrapModes, "wrap mode");
  wrap.t = *(WrapMode*) luax_optenum(L, 3, luaL_checkstring(L, 2), &WrapModes, "wrap mode");
  wrap.r = *(WrapMode*) luax_optenum(L, 4, luaL_checkstring(L, 2), &WrapModes, "wrap mode");
  lovrTextureSetWrap(texture, wrap);
  return 0;
}

const luaL_Reg lovrTexture[] = {
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
