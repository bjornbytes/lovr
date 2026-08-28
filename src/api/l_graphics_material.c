#include "api.h"
#include "graphics/graphics.h"
#include "util.h"

Material* luax_optmaterial(lua_State* L, int index) {
  if (lua_isnoneornil(L, index)) {
    return NULL;
  } else {
    Texture* texture = luax_totype(L, index, Texture);
    if (texture) {
      Material* material = lovrTextureToMaterial(texture);
      luax_assert(L, material);
      return material;
    } else {
      return luax_checktype(L, index, Material);
    }
  }
}

static int l_lovrMaterialGetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float color[4];
  lovrMaterialGetColor(material, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrMaterialSetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float color[4];
  luax_readcolor(L, 2, color);
  luax_assert(L, lovrMaterialSetColor(material, color));
  return 0;
}

static int l_lovrMaterialGetGlow(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float color[4];
  lovrMaterialGetGlow(material, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrMaterialSetGlow(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float glow[4];
  luax_readcolor(L, 2, glow);
  luax_assert(L, lovrMaterialSetGlow(material, glow));
  return 0;
}

static int l_lovrMaterialGetQuad(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float quad[4];
  lovrMaterialGetQuad(material, quad);
  lua_pushnumber(L, quad[0]);
  lua_pushnumber(L, quad[1]);
  lua_pushnumber(L, quad[2]);
  lua_pushnumber(L, quad[3]);
  return 4;
}

static int l_lovrMaterialSetQuad(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float quad[4];
  quad[0] = luax_checkfloat(L, 2);
  quad[1] = luax_checkfloat(L, 3);
  quad[2] = luax_checkfloat(L, 4);
  quad[3] = luax_checkfloat(L, 5);
  lovrMaterialSetQuad(material, quad);
  return 0;
}

static int l_lovrMaterialGetMetalness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetMetalness(material));
  return 1;
}

static int l_lovrMaterialSetMetalness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetMetalness(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetRoughness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetRoughness(material));
  return 1;
}

static int l_lovrMaterialSetRoughness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetRoughness(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetClearcoat(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetClearcoat(material));
  return 1;
}

static int l_lovrMaterialSetClearcoat(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetClearcoat(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetClearcoatRoughness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetClearcoatRoughness(material));
  return 1;
}

static int l_lovrMaterialSetClearcoatRoughness(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetClearcoatRoughness(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetOcclusionStrength(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetOcclusionStrength(material));
  return 1;
}

static int l_lovrMaterialSetOcclusionStrength(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetOcclusionStrength(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetNormalScale(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetNormalScale(material));
  return 1;
}

static int l_lovrMaterialSetNormalScale(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetNormalScale(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialGetAlphaCutoff(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushnumber(L, lovrMaterialGetAlphaCutoff(material));
  return 1;
}

static int l_lovrMaterialSetAlphaCutoff(lua_State * L) {
  Material* material = luax_checktype(L, 1, Material);
  luax_assert(L, lovrMaterialSetAlphaCutoff(material, luax_checkfloat(L, 2)));
  return 0;
}

static int l_lovrMaterialIsDoubleSided(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  lua_pushboolean(L, lovrMaterialIsDoubleSided(material));
  return 1;
}

static int l_lovrMaterialSetDoubleSided(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  bool doubleSided = lua_toboolean(L, 2);
  lovrMaterialSetDoubleSided(material, doubleSided);
  return 0;
}

static int l_lovrMaterialGetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture type = luax_checkenum(L, 2, MaterialTexture, "color");
  Texture* texture = lovrMaterialGetTexture(material, type);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrMaterialSetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  int index = 2;
  MaterialTexture type = lua_type(L, index) == LUA_TSTRING ? luax_checkenum(L, index++, MaterialTexture, NULL) : TEXTURE_COLOR;
  Texture* texture = luax_checktype(L, index, Texture);
  luax_assert(L, lovrMaterialSetTexture(material, type, texture));
  return 0;
}

static void luax_pushcolor(lua_State* L, const float color[4]) {
  lua_createtable(L, 4, 0);
  lua_pushnumber(L, color[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, color[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, color[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, color[3]);
  lua_rawseti(L, -2, 4);
}

static int l_lovrMaterialGetProperties(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);

  lua_newtable(L);

  float color[4];
  lovrMaterialGetColor(material, color);
  luax_pushcolor(L, color);
  lua_setfield(L, -2, "color");

  lovrMaterialGetGlow(material, color);
  luax_pushcolor(L, color);
  lua_setfield(L, -2, "glow");

  float quad[4];
  lovrMaterialGetQuad(material, quad);

  // Deprecated
  lua_createtable(L, 2, 0);
  lua_pushnumber(L, quad[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, quad[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvShift");

  // Deprecated
  lua_createtable(L, 2, 0);
  lua_pushnumber(L, quad[2]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, quad[3]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvScale");

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, quad[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, quad[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, quad[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, quad[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "quad");

  lua_pushnumber(L, lovrMaterialGetMetalness(material)), lua_setfield(L, -2, "metalness");
  lua_pushnumber(L, lovrMaterialGetRoughness(material)), lua_setfield(L, -2, "roughness");
  lua_pushnumber(L, lovrMaterialGetClearcoat(material)), lua_setfield(L, -2, "clearcoat");
  lua_pushnumber(L, lovrMaterialGetClearcoatRoughness(material)), lua_setfield(L, -2, "clearcoatRoughness");
  lua_pushnumber(L, lovrMaterialGetOcclusionStrength(material)), lua_setfield(L, -2, "occlusionStrength");
  lua_pushnumber(L, lovrMaterialGetNormalScale(material)), lua_setfield(L, -2, "normalScale");
  lua_pushnumber(L, lovrMaterialGetAlphaCutoff(material)), lua_setfield(L, -2, "alphaCutoff");
  lua_pushboolean(L, lovrMaterialIsDoubleSided(material)), lua_setfield(L, -2, "doubleSided");

  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_COLOR)), lua_setfield(L, -2, "texture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_GLOW)), lua_setfield(L, -2, "glowTexture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_METALNESS)), lua_setfield(L, -2, "metalnessTexture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_ROUGHNESS)), lua_setfield(L, -2, "roughnessTexture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_CLEARCOAT)), lua_setfield(L, -2, "clearcoatTexture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_OCCLUSION)), lua_setfield(L, -2, "occlusionTexture");
  luax_pushtype(L, Texture, lovrMaterialGetTexture(material, TEXTURE_NORMAL)), lua_setfield(L, -2, "normalTexture");

  return 1;
}

const luaL_Reg lovrMaterial[] = {
  { "getColor", l_lovrMaterialGetColor },
  { "setColor", l_lovrMaterialSetColor },
  { "getGlow", l_lovrMaterialGetGlow },
  { "setGlow", l_lovrMaterialSetGlow },
  { "getQuad", l_lovrMaterialGetQuad },
  { "setQuad", l_lovrMaterialSetQuad },
  { "getMetalness", l_lovrMaterialGetMetalness },
  { "setMetalness", l_lovrMaterialSetMetalness },
  { "getRoughness", l_lovrMaterialGetRoughness },
  { "setRoughness", l_lovrMaterialSetRoughness },
  { "getClearcoat", l_lovrMaterialGetClearcoat },
  { "setClearcoat", l_lovrMaterialSetClearcoat },
  { "getClearcoatRoughness", l_lovrMaterialGetClearcoatRoughness },
  { "setClearcoatRoughness", l_lovrMaterialSetClearcoatRoughness },
  { "getOcclusionStrength", l_lovrMaterialGetOcclusionStrength },
  { "setOcclusionStrength", l_lovrMaterialSetOcclusionStrength },
  { "getNormalScale", l_lovrMaterialGetNormalScale },
  { "setNormalScale", l_lovrMaterialSetNormalScale },
  { "getAlphaCutoff", l_lovrMaterialGetAlphaCutoff },
  { "setAlphaCutoff", l_lovrMaterialSetAlphaCutoff },
  { "isDoubleSided", l_lovrMaterialIsDoubleSided },
  { "setDoubleSided", l_lovrMaterialSetDoubleSided },
  { "getTexture", l_lovrMaterialGetTexture },
  { "setTexture", l_lovrMaterialSetTexture },
  { "getProperties", l_lovrMaterialGetProperties },
  { NULL, NULL }
};
