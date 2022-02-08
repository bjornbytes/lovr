#include "api.h"
#include "data/image.h"
#include "data/blob.h"
#include <lua.h>
#include <lauxlib.h>

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
  uint32_t dx = luax_optu32(L, 3, 0);
  uint32_t dy = luax_optu32(L, 4, 0);
  uint32_t sx = luax_optu32(L, 5, 0);
  uint32_t sy = luax_optu32(L, 6, 0);
  uint32_t w = luax_optu32(L, 7, source->width);
  uint32_t h = luax_optu32(L, 8, source->height);
  lovrImagePaste(image, source, dx, dy, sx, sy, w, h);
  return 0;
}

static int l_lovrImageGetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  int x = luax_checku32(L, 2);
  int y = luax_checku32(L, 3);
  Color color = lovrImageGetPixel(image, x, y);
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

static int l_lovrImageSetPixel(lua_State* L) {
  Image* image = luax_checktype(L, 1, Image);
  int x = luax_checku32(L, 2);
  int y = luax_checku32(L, 3);
  Color color = {
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
