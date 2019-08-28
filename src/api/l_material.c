#include "api.h"
#include "graphics/material.h"
#include "graphics/texture.h"

static int l_lovrMaterialGetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialColor colorType = luaL_checkoption(L, 2, "diffuse", MaterialColors);
  Color color = lovrMaterialGetColor(material, colorType);
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

static int l_lovrMaterialSetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialColor colorType = COLOR_DIFFUSE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    colorType = luaL_checkoption(L, index, NULL, MaterialColors);
    index++;
  }
  Color color;
  luax_readcolor(L, index, &color);
  lovrMaterialSetColor(material, colorType, color);
  return 0;
}

static int l_lovrMaterialGetScalar(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialScalar scalarType = luaL_checkoption(L, 2, NULL, MaterialScalars);
  float value = lovrMaterialGetScalar(material, scalarType);
  lua_pushnumber(L, value);
  return 1;
}

static int l_lovrMaterialSetScalar(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialScalar scalarType = luaL_checkoption(L, 2, NULL, MaterialScalars);
  float value = luax_checkfloat(L, 3);
  lovrMaterialSetScalar(material, scalarType, value);
  return 0;
}

static int l_lovrMaterialGetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture textureType = luaL_checkoption(L, 2, "diffuse", MaterialTextures);
  Texture* texture = lovrMaterialGetTexture(material, textureType);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrMaterialSetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture textureType = TEXTURE_DIFFUSE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    textureType = luaL_checkoption(L, index, NULL, MaterialTextures);
    index++;
  }
  Texture* texture = lua_isnoneornil(L, index) ? NULL : luax_checktype(L, index, Texture);
  lovrMaterialSetTexture(material, textureType, texture);
  return 0;
}

static int l_lovrMaterialGetTransform(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float ox, oy, sx, sy, angle;
  lovrMaterialGetTransform(material, &ox, &oy,  &sx, &sy, &angle);
  lua_pushnumber(L, ox);
  lua_pushnumber(L, oy);
  lua_pushnumber(L, sx);
  lua_pushnumber(L, sy);
  lua_pushnumber(L, angle);
  return 5;
}

static int l_lovrMaterialSetTransform(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float ox = luax_optfloat(L, 2, 0.f);
  float oy = luax_optfloat(L, 3, 0.f);
  float sx = luax_optfloat(L, 4, 1.f);
  float sy = luax_optfloat(L, 5, sx);
  float angle = luax_optfloat(L, 6, 0.f);
  lovrMaterialSetTransform(material, ox, oy, sx, sy, angle);
  return 0;
}

const luaL_Reg lovrMaterial[] = {
  { "getColor", l_lovrMaterialGetColor },
  { "setColor", l_lovrMaterialSetColor },
  { "getScalar", l_lovrMaterialGetScalar },
  { "setScalar", l_lovrMaterialSetScalar },
  { "getTexture", l_lovrMaterialGetTexture },
  { "setTexture", l_lovrMaterialSetTexture },
  { "getTransform", l_lovrMaterialGetTransform },
  { "setTransform", l_lovrMaterialSetTransform },
  { NULL, NULL }
};
