#include "api.h"
#include "graphics/graphics.h"
#include "core/util.h"
#include "data/blob.h"
#include "data/image.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static int l_lovrTextureNewView(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  TextureViewInfo info = { .parent = texture };
  info.type = luax_checkenum(L, 2, TextureType, NULL);
  info.layerIndex = luaL_optinteger(L, 3, 1) - 1;
  info.layerCount = luaL_optinteger(L, 4, 1);
  info.levelIndex = luaL_optinteger(L, 5, 1) - 1;
  info.levelCount = luaL_optinteger(L, 6, 0);
  Texture* view = lovrTextureCreateView(&info);
  luax_pushtype(L, Texture, view);
  lovrRelease(view, lovrTextureDestroy);
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
  lua_pushinteger(L, info->size[0]);
  return 1;
}

static int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->size[1]);
  return 1;
}

static int l_lovrTextureGetDepth(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->size[2]);
  return 1;
}

static int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  lua_pushinteger(L, info->size[0]);
  lua_pushinteger(L, info->size[1]);
  lua_pushinteger(L, info->size[2]);
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

static int l_lovrTextureGetParent(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luax_pushtype(L, Texture, info->parent);
  return 1;
}

static int l_lovrTextureHasFlags(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luaL_checkany(L, 2);
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++) {
    int bit = luax_checkenum(L, i, TextureFlag, NULL);
    if (~info->flags & (1 << bit)) {
      lua_pushboolean(L, false);
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrTextureWrite(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  Image* image = luax_checktype(L, 2, Image);
  uint16_t srcOffset[2], dstOffset[4], extent[2];
  dstOffset[0] = luaL_optinteger(L, 3, 0);
  dstOffset[1] = luaL_optinteger(L, 4, 0);
  dstOffset[2] = luaL_optinteger(L, 5, 1);
  dstOffset[3] = luaL_optinteger(L, 6, 1);
  srcOffset[0] = luaL_optinteger(L, 7, 0);
  srcOffset[1] = luaL_optinteger(L, 8, 0);
  extent[0] = luaL_optinteger(L, 9, 0);
  extent[1] = luaL_optinteger(L, 10, 0);
  lovrTexturePaste(texture, image, srcOffset, dstOffset, extent);
  return 0;
}

typedef struct {
  lua_State* L;
  int ref;
  TextureFormat format;
  uint32_t width;
  uint32_t height;
} luax_readback;

static void onReadback(void* data, uint64_t size, void* context) {
  luax_readback* readback = context;

  Image* image = lovrImageCreate(readback->width, readback->height, NULL, 0, readback->format);
  memcpy(image->blob->data, data, size);

  lua_rawgeti(readback->L, LUA_REGISTRYINDEX, readback->ref);
  luax_pushtype(readback->L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  lua_call(readback->L, 1, 0);
  free(readback);
}

static int l_lovrTextureRead(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* info = lovrTextureGetInfo(texture);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  uint32_t x = luaL_optinteger(L, 3, 0);
  uint32_t y = luaL_optinteger(L, 4, 0);
  uint32_t w = luaL_optinteger(L, 5, info->size[0] - x);
  uint32_t h = luaL_optinteger(L, 6, info->size[1] - y);
  uint32_t layer = luaL_optinteger(L, 7, 1);
  uint32_t level = luaL_optinteger(L, 8, 1);
  luax_readback* readback = malloc(sizeof(luax_readback));
  lovrAssert(readback, "Out of memory");
  readback->L = L;
  lua_pushvalue(L, 2);
  readback->ref = luaL_ref(L, LUA_REGISTRYINDEX);
  readback->format = info->format;
  readback->width = w;
  readback->height = h;
  uint16_t offset[4] = { x, y, layer, level };
  uint16_t extent[3] = { w, h, 1 };
  lovrTextureRead(texture, offset, extent, onReadback, readback);
  return 0;
}

static int l_lovrTextureCopy(lua_State* L) {
  Texture* src = luax_checktype(L, 1, Texture);
  Texture* dst = luax_checktype(L, 2, Texture);
  uint16_t srcOffset[4], dstOffset[4], extent[3];
  srcOffset[0] = luaL_optinteger(L, 3, 0);
  srcOffset[1] = luaL_optinteger(L, 4, 0);
  srcOffset[2] = luaL_optinteger(L, 5, 1);
  srcOffset[3] = luaL_optinteger(L, 6, 1);
  dstOffset[0] = luaL_optinteger(L, 7, 0);
  dstOffset[1] = luaL_optinteger(L, 8, 0);
  dstOffset[2] = luaL_optinteger(L, 9, 1);
  dstOffset[3] = luaL_optinteger(L, 10, 1);
  extent[0] = luaL_optinteger(L, 11, 1);
  extent[1] = luaL_optinteger(L, 12, 1);
  extent[2] = luaL_optinteger(L, 13, 1);
  lovrTextureCopy(src, dst, srcOffset, dstOffset, extent);
  return 0;
}

static int l_lovrTextureClear(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  uint16_t layer = luaL_optinteger(L, 2, 1);
  uint16_t level = luaL_optinteger(L, 3, 1);
  uint16_t layerCount = luaL_optinteger(L, 4, 0);
  uint16_t levelCount = luaL_optinteger(L, 5, 0);
  lovrTextureClear(texture, layer, level, layerCount, levelCount);
  return 0;
}

const luaL_Reg lovrTexture[] = {
  { "newView", l_lovrTextureNewView },
  { "getType", l_lovrTextureGetType },
  { "getFormat", l_lovrTextureGetFormat },
  { "getWidth", l_lovrTextureGetWidth },
  { "getHeight", l_lovrTextureGetHeight },
  { "getDepth", l_lovrTextureGetDepth },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getMipmapCount", l_lovrTextureGetMipmapCount },
  { "getSampleCount", l_lovrTextureGetSampleCount },
  { "getParent", l_lovrTextureGetParent },
  { "hasFlags", l_lovrTextureHasFlags },
  { "write", l_lovrTextureWrite },
  { "read", l_lovrTextureRead },
  { "copy", l_lovrTextureCopy },
  { "clear", l_lovrTextureClear },
  { NULL, NULL }
};
