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

static int l_lovrMaterialGetProperties(lua_State* L) {
  Material* material = luax_checktype(L, 1, Material);
  const MaterialInfo* info = lovrMaterialGetInfo(material);
  lua_newtable(L);

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, info->data.color[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, info->data.color[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, info->data.color[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, info->data.color[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "color");

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, info->data.glow[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, info->data.glow[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, info->data.glow[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, info->data.glow[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "glow");

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, info->data.uvShift[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, info->data.uvShift[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvShift");

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, info->data.uvScale[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, info->data.uvScale[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvScale");

  lua_pushnumber(L, info->data.metalness), lua_setfield(L, -2, "metalness");
  lua_pushnumber(L, info->data.roughness), lua_setfield(L, -2, "roughness");
  lua_pushnumber(L, info->data.clearcoat), lua_setfield(L, -2, "clearcoat");
  lua_pushnumber(L, info->data.clearcoatRoughness), lua_setfield(L, -2, "clearcoatRoughness");
  lua_pushnumber(L, info->data.occlusionStrength), lua_setfield(L, -2, "occlusionStrength");
  lua_pushnumber(L, info->data.normalScale), lua_setfield(L, -2, "normalScale");
  lua_pushnumber(L, info->data.alphaCutoff), lua_setfield(L, -2, "alphaCutoff");
  luax_pushtype(L, Texture, info->texture), lua_setfield(L, -2, "texture");
  luax_pushtype(L, Texture, info->glowTexture), lua_setfield(L, -2, "glowTexture");
  luax_pushtype(L, Texture, info->metalnessTexture), lua_setfield(L, -2, "metalnessTexture");
  luax_pushtype(L, Texture, info->roughnessTexture), lua_setfield(L, -2, "roughnessTexture");
  luax_pushtype(L, Texture, info->clearcoatTexture), lua_setfield(L, -2, "clearcoatTexture");
  luax_pushtype(L, Texture, info->occlusionTexture), lua_setfield(L, -2, "occlusionTexture");
  luax_pushtype(L, Texture, info->normalTexture), lua_setfield(L, -2, "normalTexture");

  return 1;
}

const luaL_Reg lovrMaterial[] = {
  { "getProperties", l_lovrMaterialGetProperties },
  { NULL, NULL }
};
