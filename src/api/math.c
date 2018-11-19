#include "api.h"
#include "api/math.h"
#include "math/math.h"
#include "math/curve.h"
#include "math/randomGenerator.h"
#include "math/transform.h"

static int l_lovrMathNewCurve(lua_State* L) {
  int top = lua_gettop(L);
  bool table = lua_type(L, 1) == LUA_TTABLE;

  if (top == 1 && !table) {
    Curve* curve = lovrCurveCreate(luaL_checkinteger(L, 1));
    luax_pushobject(L, curve);
    return 1;
  }

  int size = ((table ? lua_objlen(L, 1) : top) + 2) / 3;
  Curve* curve = lovrCurveCreate(size);
  for (int i = 0; i < size; i++) {
    float point[3];

    if (table) {
      lua_rawgeti(L, 1, 3 * i + 1);
      lua_rawgeti(L, 1, 3 * i + 2);
      lua_rawgeti(L, 1, 3 * i + 3);

      point[0] = lua_tonumber(L, -3);
      point[1] = lua_tonumber(L, -2);
      point[2] = lua_tonumber(L, -1);

      lovrCurveAddPoint(curve, point, i);
      lua_pop(L, 3);
    } else {
      point[0] = lua_tonumber(L, 3 * i + 1);
      point[1] = lua_tonumber(L, 3 * i + 2);
      point[2] = lua_tonumber(L, 3 * i + 3);
      lovrCurveAddPoint(curve, point, i);
    }
  }

  luax_pushobject(L, curve);
  return 1;
}

static int l_lovrMathNewRandomGenerator(lua_State* L) {
  RandomGenerator* generator = lovrRandomGeneratorCreate();
  if (lua_gettop(L) > 0){
    Seed seed = luax_checkrandomseed(L, 1);
    lovrRandomGeneratorSetSeed(generator, seed);
  }
  luax_pushobject(L, generator);
  lovrRelease(generator);
  return 1;
}

static int l_lovrMathNewTransform(lua_State* L) {
  float matrix[16];
  luax_readtransform(L, 1, matrix, 3);
  Transform* transform = lovrTransformCreate(matrix);
  luax_pushobject(L, transform);
  lovrRelease(transform);
  return 1;
}

static int l_lovrMathLookAt(lua_State* L) {
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

static int l_lovrMathOrientationToDirection(lua_State* L) {
  float angle = luaL_checknumber(L, 1);
  float ax = luaL_optnumber(L, 2, 0);
  float ay = luaL_optnumber(L, 3, 1);
  float az = luaL_optnumber(L, 4, 0);
  float v[3];
  lovrMathOrientationToDirection(angle, ax, ay, az, v);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

static int l_lovrMathNoise(lua_State* L) {
  switch (lua_gettop(L)) {
    case 0:
    case 1: lua_pushnumber(L, lovrMathNoise1(luaL_checknumber(L, 1))); return 1;
    case 2: lua_pushnumber(L, lovrMathNoise2(luaL_checknumber(L, 1), luaL_checknumber(L, 2))); return 1;
    case 3: lua_pushnumber(L, lovrMathNoise3(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3))); return 1;
    case 4:
    default:
      lua_pushnumber(L, lovrMathNoise4(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4)));
      return 1;
  }
}

static int l_lovrMathRandom(lua_State* L) {
  luax_pushobject(L, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandom(L);
}

static int l_lovrMathRandomNormal(lua_State* L) {
  luax_pushobject(L, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandomNormal(L);
}

static int l_lovrMathGetRandomSeed(lua_State* L) {
  luax_pushobject(L, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorGetSeed(L);
}

static int l_lovrMathSetRandomSeed(lua_State* L) {
  luax_pushobject(L, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorSetSeed(L);
}

static int l_lovrMathGammaToLinear(lua_State* L) {
  if (lua_istable(L, 1)) {
    for (int i = 0; i < 3; i++) {
      lua_rawgeti(L, 1, i + 1);
      lua_pushnumber(L, lovrMathGammaToLinear(luaL_checknumber(L, -1)));
    }
    lua_pop(L, 3);
    return 3;
  } else {
    int n = CLAMP(lua_gettop(L), 1, 3);
    for (int i = 0; i < n; i++) {
      lua_pushnumber(L, lovrMathGammaToLinear(luaL_checknumber(L, i + 1)));
    }
    return n;
  }
}

static int l_lovrMathLinearToGamma(lua_State* L) {
  if (lua_istable(L, 1)) {
    for (int i = 0; i < 3; i++) {
      lua_rawgeti(L, 1, i + 1);
      lua_pushnumber(L, lovrMathLinearToGamma(luaL_checknumber(L, -1)));
    }
    lua_pop(L, 3);
    return 3;
  } else {
    int n = CLAMP(lua_gettop(L), 1, 3);
    for (int i = 0; i < n; i++) {
      lua_pushnumber(L, lovrMathLinearToGamma(luaL_checknumber(L, i + 1)));
    }
    return n;
  }
}

static const luaL_Reg lovrMath[] = {
  { "newCurve", l_lovrMathNewCurve },
  { "newRandomGenerator", l_lovrMathNewRandomGenerator },
  { "newTransform", l_lovrMathNewTransform },
  { "orientationToDirection", l_lovrMathOrientationToDirection },
  { "lookAt", l_lovrMathLookAt },
  { "noise", l_lovrMathNoise },
  { "random", l_lovrMathRandom },
  { "randomNormal", l_lovrMathRandomNormal },
  { "getRandomSeed", l_lovrMathGetRandomSeed },
  { "setRandomSeed", l_lovrMathSetRandomSeed },
  { "gammaToLinear", l_lovrMathGammaToLinear },
  { "linearToGamma", l_lovrMathLinearToGamma },
  { NULL, NULL }
};

int luaopen_lovr_math(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, "Curve", lovrCurve);
  luax_registertype(L, "RandomGenerator", lovrRandomGenerator);
  luax_registertype(L, "Transform", lovrTransform);
  if (lovrMathInit()) {
    luax_atexit(L, lovrMathDestroy);
  }
  return 1;
}
