#include "api.h"
#include "data/data.h"

int l_lovrDataInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrData);
  luax_registertype(L, "TextureData", lovrTextureData);
  return 1;
}

int l_lovrDataNewTextureData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Texture");
  TextureData* textureData = lovrTextureDataFromBlob(blob);
  lovrRelease(&blob->ref);
  lovrRelease(&textureData->ref);
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

const luaL_Reg lovrData[] = {
  { "newTextureData", l_lovrDataNewTextureData },
  { NULL, NULL }
};
