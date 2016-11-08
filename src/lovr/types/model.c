#include "model.h"
#include "texture.h"

void luax_pushmodel(lua_State* L, Model* model) {
  if (model == NULL) {
    lua_pushnil(L);
    return;
  }

  Model** userdata = (Model**) lua_newuserdata(L, sizeof(Model*));
  luaL_getmetatable(L, "Model");
  lua_setmetatable(L, -2);
  *userdata = model;
}

Model* luax_checkmodel(lua_State* L, int index) {
  return *(Model**) luaL_checkudata(L, index, "Model");
}

int luax_destroymodel(lua_State* L) {
  return 0;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "getTexture", l_lovrModelGetTexture },
  { "setTexture", l_lovrModelSetTexture },
  { NULL, NULL }
};

int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
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
  Model* model = luax_checkmodel(L, 1);
  luax_pushtexture(L, lovrModelGetTexture(model));
  return 1;
}

int l_lovrModelSetTexture(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
  Texture* texture = luax_checktexture(L, 2);
  lovrModelSetTexture(model, texture);
  return 0;
}
