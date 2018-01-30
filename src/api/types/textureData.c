#include "api.h"

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

const luaL_Reg lovrTextureData[] = {
  { "getWidth", l_lovrTextureDataGetWidth },
  { "getHeight", l_lovrTextureDataGetHeight },
  { "getDimensions", l_lovrTextureDataGetDimensions },
  { NULL, NULL }
};
