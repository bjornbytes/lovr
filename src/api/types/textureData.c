#include "api.h"

int l_lovrTextureDataEncode(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  const char* filename = luaL_checkstring(L, 2);
  bool success = lovrTextureDataEncode(textureData, filename);
  lua_pushboolean(L, success);
  return 1;
}

int l_lovrTextureDataGetWidth(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->width);
  return 1;
}

int l_lovrTextureDataGetHeight(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->height);
  return 1;
}

int l_lovrTextureDataGetDimensions(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  lua_pushinteger(L, textureData->width);
  lua_pushinteger(L, textureData->height);
  return 2;
}

int l_lovrTextureDataGetPixel(lua_State* L) {
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

int l_lovrTextureDataSetPixel(lua_State* L) {
  TextureData* textureData = luax_checktype(L, 1, TextureData);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  Color color = {
    luaL_checknumber(L, 4),
    luaL_checknumber(L, 5),
    luaL_checknumber(L, 6),
    luaL_optnumber(L, 7, 1.f)
  };
  lovrTextureDataSetPixel(textureData, x, y, color);
  return 0;
}

const luaL_Reg lovrTextureData[] = {
  { "encode", l_lovrTextureDataEncode },
  { "getWidth", l_lovrTextureDataGetWidth },
  { "getHeight", l_lovrTextureDataGetHeight },
  { "getDimensions", l_lovrTextureDataGetDimensions },
  { "getPixel", l_lovrTextureDataGetPixel },
  { "setPixel", l_lovrTextureDataSetPixel },
  { NULL, NULL }
};
