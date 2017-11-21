#include "api/lovr.h"
#include "graphics/model.h"

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  luax_readtransform(L, 2, transform, 1);
  lovrModelDraw(model, transform);
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
  luax_pushtype(L, Animator, animator);
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

int l_lovrModelGetMesh(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Mesh* mesh = lovrModelGetMesh(model);
  luax_pushtype(L, Mesh, mesh);
  return 1;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getAABB", l_lovrModelGetAABB },
  { "getAnimator", l_lovrModelGetAnimator },
  { "setAnimator", l_lovrModelSetAnimator },
  { "getAnimationCount", l_lovrModelGetAnimationCount },
  { "getMesh", l_lovrModelGetMesh },
  { NULL, NULL }
};
