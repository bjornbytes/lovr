#include "api.h"
#include "data/textureData.h"

static int l_lovrTextureDataEncode(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  const char* filename = luaL_checkstring(L, 2);
  bool success = lovrTextureDataEncode(textureData, filename);
  lua_pushboolean(L, success);
  return 1;
}

static int l_lovrTextureDataGetWidth(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->width);
  return 1;
}

static int l_lovrTextureDataGetHeight(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->height);
  return 1;
}

static int l_lovrTextureDataGetDimensions(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->width);
  lua_pushinteger(L, textureData->height);
  return 2;
}

static int l_lovrTextureDataGetFormat(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  luax_pushenum(L, TextureFormats, textureData->format);
  return 1;
}

static int l_lovrTextureDataPaste(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  TextureData* source = luax_checktype(L, 2, TextureData);
  uint32_t dx = luaL_optinteger(L, 3, 0);
  uint32_t dy = luaL_optinteger(L, 4, 0);
  uint32_t sx = luaL_optinteger(L, 5, 0);
  uint32_t sy = luaL_optinteger(L, 6, 0);
  uint32_t w = luaL_optinteger(L, 7, source->width);
  uint32_t h = luaL_optinteger(L, 8, source->height);
  lovrTextureDataPaste(textureData, source, dx, dy, sx, sy, w, h);
  return 0;
}

static int l_lovrTextureDataGetPixel(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  Color color = lovrTextureDataGetPixel(textureData, x, y);
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

static int l_lovrTextureDataSetPixel(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  Color color = {
    luax_optfloat(L, 4, 1.f),
    luax_optfloat(L, 5, 1.f),
    luax_optfloat(L, 6, 1.f),
    luax_optfloat(L, 7, 1.f)
  };
  lovrTextureDataSetPixel(textureData, x, y, color);
  return 0;
}

static int l_lovrTextureDataGetBlob(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  Blob* blob = textureData->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}

const luaL_Reg lovrTextureData[] = {
  { "encode", l_lovrTextureDataEncode },
  { "getWidth", l_lovrTextureDataGetWidth },
  { "getHeight", l_lovrTextureDataGetHeight },
  { "getDimensions", l_lovrTextureDataGetDimensions },
  { "getFormat", l_lovrTextureDataGetFormat },
  { "paste", l_lovrTextureDataPaste },
  { "getPixel", l_lovrTextureDataGetPixel },
  { "setPixel", l_lovrTextureDataSetPixel },
  { "getBlob", l_lovrTextureDataGetBlob },
  { NULL, NULL }
};
