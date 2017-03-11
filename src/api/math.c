#include "api/lovr.h"
#include "math/mat4.h"
#include "math/transform.h"

int l_lovrMathInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, "Transform", lovrTransform);
  return 1;
}

int l_lovrMathNewTransform(lua_State* L) {
  float matrix[16];
  luax_readtransform(L, 1, matrix);
  Transform* transform = lovrTransformCreate(matrix);
  luax_pushtype(L, Transform, transform);
  lovrRelease(&transform->ref);
  return 1;
}

const luaL_Reg lovrMath[] = {
  { "newTransform", l_lovrMathNewTransform },
  { NULL, NULL }
};

