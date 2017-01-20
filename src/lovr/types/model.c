#include "lovr/types/model.h"
#include "lovr/types/texture.h"
#include "lovr/types/transform.h"

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getTexture", l_lovrModelGetTexture },
  { "setTexture", l_lovrModelSetTexture },
  { NULL, NULL }
};

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  luax_readtransform(L, 2, transform);
  lovrModelDraw(model, transform);
  return 0;
}

int l_lovrModelGetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  luax_pushtype(L, Texture, lovrModelGetTexture(model));
  return 1;
}

int l_lovrModelSetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Texture* texture = luax_checktype(L, 2, Texture);
  lovrModelSetTexture(model, texture);
  return 0;
}
