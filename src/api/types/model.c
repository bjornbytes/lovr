#include "api/lovr.h"
#include "graphics/model.h"

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  luax_readtransform(L, 2, transform, 1);
  lovrModelDraw(model, transform);
  return 0;
}

int l_lovrModelGetMesh(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Mesh* mesh = lovrModelGetMesh(model);
  luax_pushtype(L, Mesh, mesh);
  return 1;
}

int l_lovrModelGetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Texture* texture = lovrModelGetTexture(model);
  luax_pushtype(L, Texture, texture);
  return 1;
}

int l_lovrModelSetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Texture* texture = luax_checktype(L, 2, Texture);
  lovrModelSetTexture(model, texture);
  return 0;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getMesh", l_lovrModelGetMesh },
  { "getTexture", l_lovrModelGetTexture },
  { "setTexture", l_lovrModelSetTexture },
  { NULL, NULL }
};
