#include "api.h"
#include "data/image.h"
#include "data/blob.h"
#include "util.h"

StringEntry lovrTextureFormat[] = {
  [FORMAT_R8] = ENTRY("r8"),
  [FORMAT_RG8] = ENTRY("rg8"),
  [FORMAT_RGBA8] = ENTRY("rgba8"),
  [FORMAT_BGRA8] = ENTRY("bgra8"),
  [FORMAT_R16] = ENTRY("r16"),
  [FORMAT_RG16] = ENTRY("rg16"),
  [FORMAT_RGBA16] = ENTRY("rgba16"),
  [FORMAT_R16F] = ENTRY("r16f"),
  [FORMAT_RG16F] = ENTRY("rg16f"),
  [FORMAT_RGBA16F] = ENTRY("rgba16f"),
  [FORMAT_R32F] = ENTRY("r32f"),
  [FORMAT_RG32F] = ENTRY("rg32f"),
  [FORMAT_RGBA32F] = ENTRY("rgba32f"),
  [FORMAT_RGB565] = ENTRY("rgb565"),
  [FORMAT_RGB5A1] = ENTRY("rgb5a1"),
  [FORMAT_RGB10A2] = ENTRY("rgb10a2"),
  [FORMAT_RG11B10F] = ENTRY("rg11b10f"),
  [FORMAT_D16] = ENTRY("d16"),
  [FORMAT_D24] = ENTRY("d24"),
  [FORMAT_D32F] = ENTRY("d32f"),
  [FORMAT_D24S8] = ENTRY("d24s8"),
  [FORMAT_D32FS8] = ENTRY("d32fs8"),
  [FORMAT_BC1] = ENTRY("bc1"),
  [FORMAT_BC2] = ENTRY("bc2"),
  [FORMAT_BC3] = ENTRY("bc3"),
  [FORMAT_BC4U] = ENTRY("bc4u"),
  [FORMAT_BC4S] = ENTRY("bc4s"),
  [FORMAT_BC5U] = ENTRY("bc5u"),
  [FORMAT_BC5S] = ENTRY("bc5s"),
  [FORMAT_BC6UF] = ENTRY("bc6uf"),
  [FORMAT_BC6SF] = ENTRY("bc6sf"),
  [FORMAT_BC7] = ENTRY("bc7"),
  [FORMAT_ASTC_4x4] = ENTRY("astc4x4"),
  [FORMAT_ASTC_5x4] = ENTRY("astc5x4"),
  [FORMAT_ASTC_5x5] = ENTRY("astc5x5"),
  [FORMAT_ASTC_6x5] = ENTRY("astc6x5"),
  [FORMAT_ASTC_6x6] = ENTRY("astc6x6"),
  [FORMAT_ASTC_8x5] = ENTRY("astc8x5"),
  [FORMAT_ASTC_8x6] = ENTRY("astc8x6"),
  [FORMAT_ASTC_8x8] = ENTRY("astc8x8"),
  [FORMAT_ASTC_10x5] = ENTRY("astc10x5"),
  [FORMAT_ASTC_10x6] = ENTRY("astc10x6"),
  [FORMAT_ASTC_10x8] = ENTRY("astc10x8"),
  [FORMAT_ASTC_10x10] = ENTRY("astc10x10"),
  [FORMAT_ASTC_12x10] = ENTRY("astc12x10"),
  [FORMAT_ASTC_12x12] = ENTRY("astc12x12"),
  { 0 }
};

static int l_lovrImageGetBlob(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  Blob* blob = lovrImageGetBlob(image);
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrImageGetPointer(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t level = luax_optu32(L, 2, 1) - 1;
  uint32_t layer = luax_optu32(L, 3, 1) - 1;
  void* pointer = lovrImageGetLayerData(image, level, layer);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrImageGetWidth(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t width = lovrImageGetWidth(image, 0);
  lua_pushinteger(L, width);
  return 1;
}

static int l_lovrImageGetHeight(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t height = lovrImageGetHeight(image, 0);
  lua_pushinteger(L, height);
  return 1;
}

static int l_lovrImageGetDimensions(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t width = lovrImageGetWidth(image, 0);
  uint32_t height = lovrImageGetHeight(image, 0);
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static int l_lovrImageGetFormat(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  TextureFormat format = lovrImageGetFormat(image);
  luax_pushenum(L, TextureFormat, format);
  return 1;
}

static int l_lovrImageGetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t x = luax_checku32(L, 2);
  uint32_t y = luax_checku32(L, 3);
  float pixel[4] = { 0.f, 0.f, 0.f, 1.f };
  luax_assert(L, lovrImageGetPixel(image, x, y, pixel));
  lua_pushnumber(L, pixel[0]);
  lua_pushnumber(L, pixel[1]);
  lua_pushnumber(L, pixel[2]);
  lua_pushnumber(L, pixel[3]);
  return 4;
}

static int l_lovrImageSetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  uint32_t x = luax_checku32(L, 2);
  uint32_t y = luax_checku32(L, 3);
  float pixel[4] = {
    luax_optfloat(L, 4, 1.f),
    luax_optfloat(L, 5, 1.f),
    luax_optfloat(L, 6, 1.f),
    luax_optfloat(L, 7, 1.f)
  };
  luax_assert(L, lovrImageSetPixel(image, x, y, pixel));
  return 0;
}

void mapPixel(void* userdata, uint32_t x, uint32_t y, float pixel[4]) {
  lua_State* L = userdata;
  lua_pushvalue(L, 2);
  lua_pushinteger(L, x);
  lua_pushinteger(L, y);
  lua_pushnumber(L, pixel[0]);
  lua_pushnumber(L, pixel[1]);
  lua_pushnumber(L, pixel[2]);
  lua_pushnumber(L, pixel[3]);
  lua_call(L, 6, 4);
  if (!lua_isnil(L, -4)) pixel[0] = luax_tofloat(L, -4);
  if (!lua_isnil(L, -3)) pixel[1] = luax_tofloat(L, -3);
  if (!lua_isnil(L, -2)) pixel[2] = luax_tofloat(L, -2);
  if (!lua_isnil(L, -1)) pixel[3] = luax_tofloat(L, -1);
  lua_pop(L, 4);
}

static int l_lovrImageMapPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  uint32_t x = luax_optu32(L, 3, 0);
  uint32_t y = luax_optu32(L, 4, 0);
  uint32_t w = luax_optu32(L, 5, lovrImageGetWidth(image, 0));
  uint32_t h = luax_optu32(L, 6, lovrImageGetHeight(image, 0));
  lua_settop(L, 2);
  luax_assert(L, lovrImageMapPixel(image, x, y, w, h, mapPixel, L));
  return 0;
}

static int l_lovrImagePaste(lua_State* L) {
  Image* dst = luax_checktype(L, 1, Image);
  Image* src = luax_checktype(L, 2, Image);
  uint32_t srcOffset[2], dstOffset[2], extent[2];
  dstOffset[0] = luax_optu32(L, 3, 0);
  dstOffset[1] = luax_optu32(L, 4, 0);
  srcOffset[0] = luax_optu32(L, 5, 0);
  srcOffset[1] = luax_optu32(L, 6, 0);
  extent[0] = luax_optu32(L, 7, lovrImageGetWidth(src, 0));
  extent[1] = luax_optu32(L, 8, lovrImageGetHeight(src, 0));
  luax_assert(L, lovrImageCopy(src, dst, srcOffset, dstOffset, extent));
  return 0;
}

static int l_lovrImageEncode(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  Blob* blob = lovrImageEncode(image);
  luax_assert(L, blob);
  luax_pushtype(L, Blob, blob);
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

const luaL_Reg lovrImage[] = {
  { "getBlob", l_lovrImageGetBlob },
  { "getPointer", l_lovrImageGetPointer },
  { "getWidth", l_lovrImageGetWidth },
  { "getHeight", l_lovrImageGetHeight },
  { "getDimensions", l_lovrImageGetDimensions },
  { "getFormat", l_lovrImageGetFormat },
  { "getPixel", l_lovrImageGetPixel },
  { "setPixel", l_lovrImageSetPixel },
  { "mapPixel", l_lovrImageMapPixel },
  { "paste", l_lovrImagePaste },
  { "encode", l_lovrImageEncode },
  { NULL, NULL }
};
