#include "api.h"
#include "graphics/material.h"
#include "graphics/model.h"
#include "data/modelData.h"

static int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  int instances = luaL_optinteger(L, index, 1);
  lovrModelDraw(model, transform, instances);
  return 0;
}

static int l_lovrModelAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);

  uint32_t animation = ~0u;
  switch (lua_type(L, 2)) {
    case LUA_TSTRING: {
      const char* name = lua_tostring(L, 2);
      ModelData* modelData = lovrModelGetModelData(model);
      uint32_t* index = map_get(&modelData->animationMap, name);
      lovrAssert(index, "Model has no animation named '%s'", name);
      animation = *index;
      break;
    }
    case LUA_TNUMBER:
      animation = lua_tointeger(L, 2) - 1;
      break;
    default:
      return luaL_typerror(L, 2, "number or string");
  }

  float time = luaL_checknumber(L, 3);
  float alpha = luax_optfloat(L, 4, 1.f);
  lovrModelAnimate(model, animation, time, alpha);
  return 0;
}

static int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Material* material = lovrModelGetMaterial(model);
  luax_pushtype(L, Material, material);
  return 1;
}

static int l_lovrModelSetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  if (lua_isnoneornil(L, 2)) {
    lovrModelSetMaterial(model, NULL);
  } else {
    Material* material = luax_checktype(L, 2, Material);
    lovrModelSetMaterial(model, material);
  }
  return 0;
}

static int l_lovrModelGetAABB(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float aabb[6];
  lovrModelGetAABB(model, aabb);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "animate", l_lovrModelAnimate },
  { "getMaterial", l_lovrModelGetMaterial },
  { "setMaterial", l_lovrModelSetMaterial },
  { "getAABB", l_lovrModelGetAABB },
  { NULL, NULL }
};
