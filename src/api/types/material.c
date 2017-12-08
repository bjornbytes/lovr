#include "api/lovr.h"
#include "graphics/material.h"

int l_lovrMaterialGetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialColor colorType = *(MaterialColor*) luax_optenum(L, 2, "diffuse", &MaterialColors, "color");
  Color color = lovrMaterialGetColor(material, colorType);
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

int l_lovrMaterialSetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialColor colorType = COLOR_DIFFUSE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    colorType = *(MaterialColor*) luax_checkenum(L, index, &MaterialColors, "color");
    index++;
  }
  Color color = luax_checkcolor(L, index);
  lovrMaterialSetColor(material, colorType, color);
  return 0;
}

int l_lovrMaterialGetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture textureType = *(MaterialTexture*) luax_optenum(L, 2, "diffuse", &MaterialTextures, "texture");
  Texture* texture = lovrMaterialGetTexture(material, textureType);
  luax_pushtype(L, Texture, texture);
  return 1;
}

int l_lovrMaterialSetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture textureType = TEXTURE_DIFFUSE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    textureType = *(MaterialTexture*) luax_checkenum(L, index, &MaterialTextures, "texture");
    index++;
  }
  Texture* texture = lua_isnoneornil(L, index) ? NULL : luax_checktypeof(L, index, Texture);
  lovrMaterialSetTexture(material, textureType, texture);
  return 0;
}

const luaL_Reg lovrMaterial[] = {
  { "getColor", l_lovrMaterialGetColor },
  { "setColor", l_lovrMaterialSetColor },
  { "getTexture", l_lovrMaterialGetTexture },
  { "setTexture", l_lovrMaterialSetTexture },
  { NULL, NULL }
};
