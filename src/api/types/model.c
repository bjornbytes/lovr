#include "api.h"
#include "api/math.h"
#include "graphics/model.h"

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  int instances = luaL_optinteger(L, index, 1);
  lovrModelDraw(model, transform, instances);
  return 0;
}

int l_lovrModelGetAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  luax_pushobject(L, lovrModelGetAnimator(model));
  return 1;
}

int l_lovrModelSetAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  if (lua_isnoneornil(L, 2)) {
    lovrModelSetAnimator(model, NULL);
  } else {
    Animator* animator = luax_checktype(L, 2, Animator);
    lovrModelSetAnimator(model, animator);
  }
  return 0;
}

int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Material* material = lovrModelGetMaterial(model);
  luax_pushobject(L, material);
  return 1;
}

int l_lovrModelSetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  if (lua_isnoneornil(L, 2)) {
    lovrModelSetMaterial(model, NULL);
  } else {
    Material* material = luax_checktype(L, 2, Material);
    lovrModelSetMaterial(model, material);
  }
  return 0;
}

int l_lovrModelGetAABB(lua_State* L) {
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
  { "getAnimator", l_lovrModelGetAnimator },
  { "setAnimator", l_lovrModelSetAnimator },
  { "getMaterial", l_lovrModelGetMaterial },
  { "setMaterial", l_lovrModelSetMaterial },
  { "getAABB", l_lovrModelGetAABB },
  { NULL, NULL }
};
