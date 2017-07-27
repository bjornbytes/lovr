#include "api/lovr.h"
#include "math/mat4.h"
#include "math/quat.h"
#include "math/randomGenerator.h"
#include "math/transform.h"

int l_lovrMathInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, "RandomGenerator", lovrRandomGenerator);
  luax_registertype(L, "Transform", lovrTransform);
  return 1;
}

int l_lovrMathNewRandomGenerator(lua_State* L) {
  RandomGenerator* generator = lovrRandomGeneratorCreate();
  if (lua_gettop(L) > 0){
    Seed seed = luax_checkrandomseed(L, 1);
    lovrRandomGeneratorSetSeed(generator, seed);
  }
  luax_pushtype(L, RandomGenerator, generator);
  lovrRelease(&generator->ref);
  return 1;
}

int l_lovrMathNewTransform(lua_State* L) {
  float matrix[16];
  luax_readtransform(L, 1, matrix, 0);
  Transform* transform = lovrTransformCreate(matrix);
  luax_pushtype(L, Transform, transform);
  lovrRelease(&transform->ref);
  return 1;
}

int l_lovrMathLookAt(lua_State* L) {
  float from[3] = { luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3) };
  float to[3] = { luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6) };
  float up[3] = { luaL_optnumber(L, 7, 0), luaL_optnumber(L, 8, 1), luaL_optnumber(L, 9, 0) };
  float m[16], q[4], angle, ax, ay, az;
  mat4_lookAt(m, from, to, up);
  quat_fromMat4(q, m);
  quat_getAngleAxis(q, &angle, &ax, &ay, &az);
  lua_pushnumber(L, -angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

const luaL_Reg lovrMath[] = {
  { "newRandomGenerator", l_lovrMathNewRandomGenerator },
  { "newTransform", l_lovrMathNewTransform },
  { "lookAt", l_lovrMathLookAt },
  { NULL, NULL }
};

