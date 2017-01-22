#include "lovr/math.h"
#include "lovr/types/transform.h"
#include "math/mat4.h"

const luaL_Reg lovrMath[] = {
  { "newTransform", l_lovrMathNewTransform },
  { NULL, NULL }
};

int l_lovrMathInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, "Transform", lovrTransform);
  return 1;
}

int l_lovrMathNewTransform(lua_State* L) {
  float transfrom[16];
  luax_readtransform(L, 1, transfrom);
  luax_pushtype(L, Transform, lovrTransformCreate(transfrom));
  return 1;
}
