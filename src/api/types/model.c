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

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { NULL, NULL }
};
