#include "api.h"
#include "api/math.h"
#include "graphics/model.h"

int l_lovrModelDrawInstanced(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  int instances = luaL_checkinteger(L, 2);
  float transform[16];
  luax_readmat4(L, 3, transform, 1, NULL);
  lovrModelDraw(model, transform, instances);
  return 0;
}

int l_lovrModelDraw(lua_State* L) {
  lua_pushinteger(L, 1);
  lua_insert(L, 2);
  return l_lovrModelDrawInstanced(L);
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
  { "drawInstanced", l_lovrModelDrawInstanced },
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
