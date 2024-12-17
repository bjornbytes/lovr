#include "api.h"
#include "util.h"
#include "graphics/graphics.h"
#include "data/image.h"
#include <string.h>

static int l_lovrTextureGetLabel(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushstring(L, info->label);
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
  lua_pushboolean(L, !info->srgb);
  return 2;
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

static int l_lovrTextureNewReadback(lua_State* L) {
  Texture* texture = luax_totype(L, 1, Texture);
  uint32_t offset[4], extent[3];
  offset[0] = luax_optu32(L, 2, 0);
  offset[1] = luax_optu32(L, 3, 0);
  offset[2] = luax_optu32(L, 4, 1) - 1;
  offset[3] = luax_optu32(L, 5, 1) - 1;
  extent[0] = luax_optu32(L, 6, ~0u);
  extent[1] = luax_optu32(L, 7, ~0u);
  extent[2] = 1;
  Readback* readback = lovrReadbackCreateTexture(texture, offset, extent);
  luax_pushtype(L, Readback, readback);
  lovrRelease(readback, lovrReadbackDestroy);
  return 1;
}

static int l_lovrTextureGetPixels(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  uint32_t offset[4], extent[3];
  offset[0] = luax_optu32(L, 2, 0);
  offset[1] = luax_optu32(L, 3, 0);
  offset[2] = luax_optu32(L, 4, 1) - 1;
  offset[3] = luax_optu32(L, 5, 1) - 1;
  extent[0] = luax_optu32(L, 6, ~0u);
  extent[1] = luax_optu32(L, 7, ~0u);
  extent[2] = 1;
  Image* image = lovrTextureGetPixels(texture, offset, extent);
  luax_assert(L, image);
  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  return 1;
}

static int l_lovrTextureSetPixels(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);

  Image* image = luax_totype(L, 2, Image);

  if (image) {
    uint32_t dstOffset[4];
    uint32_t srcOffset[4];
    uint32_t extent[3];
    dstOffset[0] = luax_optu32(L, 3, 0);
    dstOffset[1] = luax_optu32(L, 4, 0);
    dstOffset[2] = luax_optu32(L, 5, 1) - 1;
    dstOffset[3] = luax_optu32(L, 6, 1) - 1;
    srcOffset[0] = luax_optu32(L, 7, 0);
    srcOffset[1] = luax_optu32(L, 8, 0);
    srcOffset[2] = luax_optu32(L, 9, 1) - 1;
    srcOffset[3] = luax_optu32(L, 10, 1) - 1;
    extent[0] = luax_optu32(L, 11, ~0u);
    extent[1] = luax_optu32(L, 12, ~0u);
    extent[2] = luax_optu32(L, 13, ~0u);
    luax_assert(L, lovrTextureSetPixels(texture, image, dstOffset, srcOffset, extent));
    return 0;
  }

  Texture* src = luax_totype(L, 2, Texture);

  if (texture) {
    Texture* dst = texture;
    uint32_t dstOffset[4];
    uint32_t srcOffset[4];
    uint32_t dstExtent[3];
    uint32_t srcExtent[3];
    dstOffset[0] = luax_optu32(L, 3, 0);
    dstOffset[1] = luax_optu32(L, 4, 0);
    dstOffset[2] = luax_optu32(L, 5, 1) - 1;
    dstOffset[3] = luax_optu32(L, 6, 1) - 1;
    srcOffset[0] = luax_optu32(L, 7, 0);
    srcOffset[1] = luax_optu32(L, 8, 0);
    srcOffset[2] = luax_optu32(L, 9, 1) - 1;
    srcOffset[3] = luax_optu32(L, 10, 1) - 1;
    dstExtent[0] = luax_optu32(L, 11, ~0u);
    dstExtent[1] = luax_optu32(L, 12, ~0u);
    dstExtent[2] = luax_optu32(L, 13, ~0u);
    srcExtent[0] = luax_optu32(L, 14, dstExtent[0]);
    srcExtent[1] = luax_optu32(L, 15, dstExtent[1]);
    srcExtent[2] = luax_optu32(L, 16, dstExtent[2]);
    FilterMode filter = luax_checkenum(L, 17, FilterMode, "linear");
    if (srcExtent[0] == dstExtent[0] && srcExtent[1] == dstExtent[1] && srcExtent[2] == dstExtent[2]) {
      luax_assert(L, lovrTextureCopy(src, dst, srcOffset, dstOffset, dstExtent));
    } else {
      luax_assert(L, lovrTextureBlit(src, dst, srcOffset, dstOffset, srcExtent, dstExtent, filter));
    }
    return 0;
  }

  return luax_typeerror(L, 2, "Image or Texture");
}

static int l_lovrTextureClear(lua_State* L) {
  Texture* texture = luax_totype(L, 1, Texture);

  int index;
  float value[4];
  if (lua_isnoneornil(L, 2)) {
    memset(value, 0, sizeof(value));
    index = 3;
  } else if (lua_type(L, 2) == LUA_TNUMBER && lua_gettop(L) > 2) {
    luax_readcolor(L, 2, value);
    index = 6;
  } else {
    luax_optcolor(L, 2, value);
    index = 3;
  }

  uint32_t layer = luax_optu32(L, index++, 1) - 1;
  uint32_t layerCount = luax_optu32(L, index++, ~0u);
  uint32_t level = luax_optu32(L, index++, 1) - 1;
  uint32_t levelCount = luax_optu32(L, index++, ~0u);
  luax_assert(L, lovrTextureClear(texture, value, layer, layerCount, level, levelCount));
  return 0;
}

static int l_lovrTextureGenerateMipmaps(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  uint32_t base = luax_optu32(L, 2, 1) - 1;
  uint32_t count = luax_optu32(L, 3, ~0u);
  luax_assert(L, lovrTextureGenerateMipmaps(texture, base, count));
  return 0;
}

static int l_lovrTextureGetSampler(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  Sampler* sampler = lovrTextureGetSampler(texture);
  luax_pushtype(L, Sampler, sampler);
  return 1;
}

static int l_lovrTextureSetSampler(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrTextureSetSampler(texture, NULL);
      return 0;
    case LUA_TSTRING:
      lovrTextureSetSampler(texture, lovrGraphicsGetDefaultSampler(luax_checkenum(L, 2, FilterMode, NULL)));
      return 0;
    case LUA_TUSERDATA:
      lovrTextureSetSampler(texture, luax_checktype(L, 2, Sampler));
      return 0;
    default: return luax_typeerror(L, 2, "nil, FilterMode, or Sampler");
  }
}

static int l_lovrTextureToString(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const char* label = lovrTextureGetInfo(texture)->label;
  if (label) {
    lua_pushfstring(L, "Texture(%s): %p", label, texture);
  } else {
    lua_pushfstring(L, "Texture: %p", texture);
  }
  return 1;
}

const luaL_Reg lovrTexture[] = {
  { "getLabel", l_lovrTextureGetLabel },
  { "getType", l_lovrTextureGetType },
  { "getFormat", l_lovrTextureGetFormat },
  { "getWidth", l_lovrTextureGetWidth },
  { "getHeight", l_lovrTextureGetHeight },
  { "getLayerCount", l_lovrTextureGetLayerCount },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getMipmapCount", l_lovrTextureGetMipmapCount },
  { "getSampleCount", l_lovrTextureGetSampleCount },
  { "hasUsage", l_lovrTextureHasUsage },
  { "newReadback", l_lovrTextureNewReadback },
  { "getPixels", l_lovrTextureGetPixels },
  { "setPixels", l_lovrTextureSetPixels },
  { "clear", l_lovrTextureClear },
  { "generateMipmaps", l_lovrTextureGenerateMipmaps },
  { "getSampler", l_lovrTextureGetSampler },
  { "setSampler", l_lovrTextureSetSampler },
  { "__tostring", l_lovrTextureToString },
  { NULL, NULL }
};
