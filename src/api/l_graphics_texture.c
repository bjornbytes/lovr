#include "api.h"
#include "util.h"
#include "graphics/graphics.h"

static int l_lovrTextureNewView(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureViewInfo info = { .parent = texture };
  if (lua_type(L, 2) == LUA_TNUMBER) {
    info.type = TEXTURE_2D;
    info.layerIndex = luax_checku32(L, 2) - 1;
    info.layerCount = 1;
    info.levelIndex = luax_optu32(L, 3, 1) - 1;
    info.levelCount = 1;
  } else if (lua_isstring(L, 2)) {
    info.type = luax_checkenum(L, 2, TextureType, NULL);
    info.layerIndex = luaL_optinteger(L, 3, 1) - 1;
    info.layerCount = luaL_optinteger(L, 4, 1);
    info.levelIndex = luaL_optinteger(L, 5, 1) - 1;
    info.levelCount = luaL_optinteger(L, 6, 0);
  }
  Texture* view = lovrTextureCreateView(&info);
  luax_pushtype(L, Texture, view);
  lovrRelease(view, lovrTextureDestroy);
  return 1;
}

static int l_lovrTextureIsView(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lua_pushboolean(L, !!lovrTextureGetInfo(texture)->parent);
  return 1;
}

static int l_lovrTextureGetParent(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luax_pushtype(L, Texture, info->parent);
  return 1;
}

static int l_lovrTextureGetType(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luax_pushenum(L, TextureType, info->type);
  return 1;
}

static int l_lovrTextureGetFormat(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luax_pushenum(L, TextureFormat, info->format);
  return 1;
}

static int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->width);
  return 1;
}

static int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->height);
  return 1;
}

static int l_lovrTextureGetLayerCount(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->layers);
  return 1;
}

static int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->width);
  lua_pushinteger(L, info->height);
  lua_pushinteger(L, info->layers);
  return 3;
}

static int l_lovrTextureGetMipmapCount(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->mipmaps);
  return 1;
}

static int l_lovrTextureGetSampleCount(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->samples);
  return 1;
}

static int l_lovrTextureHasUsage(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luaL_checkany(L, 2);
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++) {
    int bit = luax_checkenum(L, i, TextureUsage, NULL);
    if (~info->usage & (1 << bit)) {
      lua_pushboolean(L, false);
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

const luaL_Reg lovrTexture[] = {
  { "newView", l_lovrTextureNewView },
  { "isView", l_lovrTextureIsView },
  { "getParent", l_lovrTextureGetParent },
  { "getType", l_lovrTextureGetType },
  { "getFormat", l_lovrTextureGetFormat },
  { "getWidth", l_lovrTextureGetWidth },
  { "getHeight", l_lovrTextureGetHeight },
  { "getLayerCount", l_lovrTextureGetLayerCount },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getMipmapCount", l_lovrTextureGetMipmapCount },
  { "getSampleCount", l_lovrTextureGetSampleCount },
  { "hasUsage", l_lovrTextureHasUsage },
  { NULL, NULL }
};
