#include "texture.h"

void luax_pushtexture(lua_State* L, Texture* texture) {
  if (texture == NULL) {
    return lua_pushnil(L);
  }

  Texture** userdata = (Texture**) lua_newuserdata(L, sizeof(Texture*));
  luaL_getmetatable(L, "Texture");
  lua_setmetatable(L, -2);
  *userdata = texture;
}

Texture* luax_checktexture(lua_State* L, int index) {
  return *(Texture**) luaL_checkudata(L, index, "Texture");
}

int luax_destroytexture(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lovrTextureDestroy(texture);
  return 0;
}

const luaL_Reg lovrTexture[] = {
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getWidth", l_lovrTextureGetWidth },
  { "getHeight", l_lovrTextureGetHeight },
  { NULL, NULL }
};

int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 2;
}

int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  return 1;
}

int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 1;
}
