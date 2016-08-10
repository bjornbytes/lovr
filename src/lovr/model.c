#include "model.h"

void luax_pushmodel(lua_State* L, Model* model) {
  if (model == NULL) {
    return lua_pushnil(L);
  }

  Model** userdata = (Model**) lua_newuserdata(L, sizeof(Model*));
  luaL_getmetatable(L, "Model");
  lua_setmetatable(L, -2);
  *userdata = model;
}

Model* luax_checkmodel(lua_State* L, int index) {
  return *(Model**) luaL_checkudata(L, index, "Model");
}

const luaL_Reg lovrModel[] = {
  { NULL, NULL }
};
