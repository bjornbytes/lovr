#include "api.h"
#include "api/math.h"
#include "graphics/model.h"

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1, NULL);
  int instances = luaL_optinteger(L, index, 1);
  lovrModelDraw(model, transform, instances);
  return 0;
}

int l_lovrModelGetAABB(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  const float* aabb = lovrModelGetAABB(model);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

int l_lovrModelGetAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Animator* animator = lovrModelGetAnimator(model);
  luax_pushobject(L, animator);
  return 1;
}

int l_lovrModelSetAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  if (lua_isnil(L, 2)) {
    lovrModelSetAnimator(model, NULL);
  } else {
    Animator* animator = luax_checktype(L, 2, Animator);
    lovrModelSetAnimator(model, animator);
  }
  return 0;
}

int l_lovrModelGetAnimationCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lua_pushinteger(L, lovrModelGetAnimationCount(model));
  return 1;
}

int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Material* material = lovrModelGetMaterial(model);
  if (material) {
    luax_pushobject(L, material);
  } else {
    lua_pushnil(L);
  }
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

int l_lovrModelGetMesh(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Mesh* mesh = lovrModelGetMesh(model);
  luax_pushobject(L, mesh);
  return 1;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getAABB", l_lovrModelGetAABB },
  { "getAnimator", l_lovrModelGetAnimator },
  { "setAnimator", l_lovrModelSetAnimator },
  { "getAnimationCount", l_lovrModelGetAnimationCount },
  { "getMaterial", l_lovrModelGetMaterial },
  { "setMaterial", l_lovrModelSetMaterial },
  { "getMesh", l_lovrModelGetMesh },
  { NULL, NULL }
};
