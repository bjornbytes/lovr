#include "api.h"
#include "graphics/texture.h"
#include <lua.h>
#include <lauxlib.h>

int luax_optmipmap(lua_State* L, int index, Texture* texture) {
  uint32_t mipmap = luaL_optinteger(L, index, 1);
  lovrAssert(mipmap <= lovrTextureGetMipmapCount(texture), "Invalid mipmap %d", mipmap);
  return mipmap - 1;
}

static int l_lovrTextureGetCompareMode(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  luax_pushenum(L, CompareMode, lovrTextureGetCompareMode(texture));
  return 1;
}

static int l_lovrTextureGetDepth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetDepth(texture, luax_optmipmap(L, 2, texture)));
  return 1;
}

static int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  int mipmap = luax_optmipmap(L, 2, texture);
  lua_pushinteger(L, lovrTextureGetWidth(texture, mipmap));
  lua_pushinteger(L, lovrTextureGetHeight(texture, mipmap));
  if (lovrTextureGetType(texture) != TEXTURE_2D) {
    lua_pushinteger(L, lovrTextureGetDepth(texture, mipmap));
    return 3;
  }
  return 2;
}

static int l_lovrTextureGetFilter(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureFilter filter = lovrTextureGetFilter(texture);
  luax_pushenum(L, FilterMode, filter.mode);
  lua_pushnumber(L, filter.anisotropy);
  return 2;
}

static int l_lovrTextureGetFormat(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  luax_pushenum(L, TextureFormat, lovrTextureGetFormat(texture));
  return 1;
}

static int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetHeight(texture, luax_optmipmap(L, 2, texture)));
  return 1;
}

static int l_lovrTextureGetMipmapCount(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushinteger(L, lovrTextureGetMipmapCount(texture));
  return 1;
}

static int l_lovrTextureGetType(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  luax_pushenum(L, TextureType, lovrTextureGetType(texture));
  return 1;
}

static int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushnumber(L, lovrTextureGetWidth(texture, luax_optmipmap(L, 2, texture)));
  return 1;
}

static int l_lovrTextureGetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureWrap wrap = lovrTextureGetWrap(texture);
  luax_pushenum(L, WrapMode, wrap.s);
  luax_pushenum(L, WrapMode, wrap.t);
  if (lovrTextureGetType(texture) == TEXTURE_CUBE) {
    luax_pushenum(L, WrapMode, wrap.r);
    return 3;
  }
  return 2;
}

static int l_lovrTextureReplacePixels(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  Image* image = luax_checktype(L, 2, Image);
  int x = luaL_optinteger(L, 3, 0);
  int y = luaL_optinteger(L, 4, 0);
  int slice = luaL_optinteger(L, 5, 1) - 1;
  int mipmap = luaL_optinteger(L, 6, 1) - 1;
  lovrTextureReplacePixels(texture, image, x, y, slice, mipmap);
  return 0;
}

static int l_lovrTextureSetCompareMode(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  CompareMode mode = lua_isnoneornil(L, 2) ? COMPARE_NONE : luax_checkenum(L, 2, CompareMode, NULL);
  lovrTextureSetCompareMode(texture, mode);
  return 0;
}

static int l_lovrTextureSetFilter(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  FilterMode mode = luax_checkenum(L, 2, FilterMode, NULL);
  float anisotropy = luax_optfloat(L, 3, 1.f);
  TextureFilter filter = { .mode = mode, .anisotropy = anisotropy };
  lovrTextureSetFilter(texture, filter);
  return 0;
}

static int l_lovrTextureSetWrap(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureWrap wrap;
  wrap.s = luax_checkenum(L, 2, WrapMode, NULL);
  wrap.t = luax_checkenum(L, 3, WrapMode, lua_tostring(L, 2));
  wrap.r = luax_checkenum(L, 4, WrapMode, lua_tostring(L, 2));
  lovrTextureSetWrap(texture, wrap);
  return 0;
}

const luaL_Reg lovrTexture[] = {
  { "getCompareMode", l_lovrTextureGetCompareMode },
  { "getDepth", l_lovrTextureGetDepth },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getFilter", l_lovrTextureGetFilter },
  { "getFormat", l_lovrTextureGetFormat },
  { "getHeight", l_lovrTextureGetHeight },
  { "getMipmapCount", l_lovrTextureGetMipmapCount },
  { "getType", l_lovrTextureGetType },
  { "getWidth", l_lovrTextureGetWidth },
  { "getWrap", l_lovrTextureGetWrap },
  { "replacePixels", l_lovrTextureReplacePixels },
  { "setCompareMode", l_lovrTextureSetCompareMode },
  { "setFilter", l_lovrTextureSetFilter },
  { "setWrap", l_lovrTextureSetWrap },
  { NULL, NULL }
};
