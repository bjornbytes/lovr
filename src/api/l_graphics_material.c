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

static int l_lovrMaterialGetNumber(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialNumber key = luax_checkenum(L, 2, MaterialNumber, NULL);
  float number = lovrMaterialGetNumber(material, key);
  lua_pushnumber(L, number);
  return 1;
}

static int l_lovrMaterialSetNumber(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialNumber key = luax_checkenum(L, 2, MaterialNumber, NULL);
  float number = luax_checkfloat(L, 3);
  luax_assert(L, lovrMaterialSetNumber(material, key, number));
  return 0;
}

static int l_lovrMaterialGetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialColor key = luax_checkenum(L, 2, MaterialColor, "base");
  const float* color = lovrMaterialGetColor(material, key);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrMaterialSetColor(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  int index = 2;
  float color[4];
  MaterialColor key = lua_type(L, index) == LUA_TSTRING ? luax_checkenum(L, index++, MaterialColor, NULL) : COLOR_BASE;
  luax_readcolor(L, index, color);
  luax_assert(L, lovrMaterialSetColor(material, key, color));
  return 0;
}

static int l_lovrMaterialGetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  MaterialTexture key = luax_checkenum(L, 2, MaterialTexture, "color");
  Texture* texture = lovrMaterialGetTexture(material, key);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrMaterialSetTexture(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  int index = 2;
  MaterialTexture key = lua_type(L, index) == LUA_TSTRING ? luax_checkenum(L, index++, MaterialTexture, NULL) : TEXTURE_COLOR;
  Texture* texture = luax_checktype(L, index, Texture);
  luax_assert(L, lovrMaterialSetTexture(material, key, texture));
  return 0;
}

static int l_lovrMaterialGetQuad(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float ox, oy, sx, sy;
  lovrMaterialGetQuad(material, &ox, &oy, &sx, &sy);
  lua_pushnumber(L, ox);
  lua_pushnumber(L, oy);
  lua_pushnumber(L, sx);
  lua_pushnumber(L, sy);
  return 4;
}

static int l_lovrMaterialSetQuad(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  float ox = luax_checkfloat(L, 2);
  float oy = luax_checkfloat(L, 3);
  float sx = luax_checkfloat(L, 4);
  float sy = luax_checkfloat(L, 5);
  lovrMaterialSetQuad(material, ox, oy, sx, sy);
  return 0;
}

// Deprecated
static int l_lovrMaterialGetProperties(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);

  lua_newtable(L);

  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_METALNESS)), lua_setfield(L, -2, "metalness");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_ROUGHNESS)), lua_setfield(L, -2, "roughness");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_CLEARCOAT)), lua_setfield(L, -2, "clearcoat");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_CLEARCOAT_ROUGHNESS)), lua_setfield(L, -2, "clearcoatRoughness");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_OCCLUSION_STRENGTH)), lua_setfield(L, -2, "occlusionStrength");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_NORMAL_SCALE)), lua_setfield(L, -2, "normalScale");
  lua_pushnumber(L, lovrMaterialGetNumber(material, NUMBER_ALPHA_CUTOFF)), lua_setfield(L, -2, "alphaCutoff");

  const float* color;
  color = lovrMaterialGetColor(material, COLOR_BASE);

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, color[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, color[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, color[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, color[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "color");

  color = lovrMaterialGetColor(material, COLOR_GLOW);

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, color[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, color[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, color[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, color[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "glow");

  float ox, oy, sx, sy;
  lovrMaterialGetQuad(material, &ox, &oy, &sx, &sy);

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, ox);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, oy);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvShift");

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, sx);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, sy);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvScale");

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
  { "getNumber", l_lovrMaterialGetNumber },
  { "setNumber", l_lovrMaterialSetNumber },
  { "getColor", l_lovrMaterialGetColor },
  { "setColor", l_lovrMaterialSetColor },
  { "getTexture", l_lovrMaterialGetTexture },
  { "setTexture", l_lovrMaterialSetTexture },
  { "getQuad", l_lovrMaterialGetQuad },
  { "setQuad", l_lovrMaterialSetQuad },

  // Deprecated
  { "getProperties", l_lovrMaterialGetProperties },

  { NULL, NULL }
};
