#include "lovr/types/model.h"
#include "lovr/types/texture.h"

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getTexture", l_lovrModelGetTexture },
  { "setTexture", l_lovrModelSetTexture },
  { NULL, NULL }
};

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float x = luaL_optnumber(L, 2, 0.f);
  float y = luaL_optnumber(L, 3, 0.f);
  float z = luaL_optnumber(L, 4, 0.f);
  float scale = luaL_optnumber(L, 5, 1.f);
  float angle = luaL_optnumber(L, 6, 0.f);
  float ax = luaL_optnumber(L, 7, 0.f);
  float ay = luaL_optnumber(L, 8, 1.f);
  float az = luaL_optnumber(L, 9, 0.f);
  lovrModelDraw(model, x, y, z, scale, angle, ax, ay, az);
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
