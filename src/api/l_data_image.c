#include "api.h"
#include "data/image.h"
#include "data/blob.h"
#include <lua.h>
#include <lauxlib.h>

StringEntry lovrTextureFormat[] = {
  [FORMAT_R8] = ENTRY("r8"),
  [FORMAT_RG8] = ENTRY("rg8"),
  [FORMAT_RGBA8] = ENTRY("rgba8"),
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
  [FORMAT_D24S8] = ENTRY("d24s8"),
  [FORMAT_D32F] = ENTRY("d32f"),
  [FORMAT_BC6] = ENTRY("bc6"),
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

static int l_lovrImageEncode(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  Blob* blob = lovrImageEncode(image);
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrImageGetWidth(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  lua_pushinteger(L, image->width);
  return 1;
}

static int l_lovrImageGetHeight(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  lua_pushinteger(L, image->height);
  return 1;
}

static int l_lovrImageGetDimensions(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  lua_pushinteger(L, image->width);
  lua_pushinteger(L, image->height);
  return 2;
}

static int l_lovrImageGetFormat(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  luax_pushenum(L, TextureFormat, image->format);
  return 1;
}

static int l_lovrImagePaste(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  Image* source = luax_checktype(L, 2, Image);
  uint32_t dx = luaL_optinteger(L, 3, 0);
  uint32_t dy = luaL_optinteger(L, 4, 0);
  uint32_t sx = luaL_optinteger(L, 5, 0);
  uint32_t sy = luaL_optinteger(L, 6, 0);
  uint32_t w = luaL_optinteger(L, 7, source->width);
  uint32_t h = luaL_optinteger(L, 8, source->height);
  lovrImagePaste(image, source, dx, dy, sx, sy, w, h);
  return 0;
}

static int l_lovrImageGetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  float color[4];
  lovrImageGetPixel(image, x, y, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrImageSetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  float color[4] = {
    luax_optfloat(L, 4, 1.f),
    luax_optfloat(L, 5, 1.f),
    luax_optfloat(L, 6, 1.f),
    luax_optfloat(L, 7, 1.f)
  };
  lovrImageSetPixel(image, x, y, color);
  return 0;
}

static int l_lovrImageGetBlob(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  Blob* blob = image->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}

const luaL_Reg lovrImage[] = {
  { "encode", l_lovrImageEncode },
  { "getWidth", l_lovrImageGetWidth },
  { "getHeight", l_lovrImageGetHeight },
  { "getDimensions", l_lovrImageGetDimensions },
  { "getFormat", l_lovrImageGetFormat },
  { "paste", l_lovrImagePaste },
  { "getPixel", l_lovrImageGetPixel },
  { "setPixel", l_lovrImageSetPixel },
  { "getBlob", l_lovrImageGetBlob },
  { NULL, NULL }
};
