#include "api.h"
#include "math/math.h"
#include "l_math.lua.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static int l_lovrMathNewCurve(lua_State* L) {
  Curve* curve = lovrCurveCreate();
  int top = lua_gettop(L);

  if (lua_istable(L, 1)) {
    int pointIndex = 0;
    int length = luax_len(L, 1);
    for (int i = 1; i <= length;) {
      lua_rawgeti(L, 1, i + 0);
      lua_rawgeti(L, 1, i + 1);
      lua_rawgeti(L, 1, i + 2);
      float point[4];
      int components = luax_readvec3(L, -3, point, "vec3 or number");
      lovrCurveAddPoint(curve, point, pointIndex++);
      i += 3 + components;
      lua_pop(L, 3);
    }
  } else if (top == 1 && lua_type(L, 1) == LUA_TNUMBER) {
    float point[4] = { 0.f };
    int count = lua_tonumber(L, 1);
    lovrCheck(count > 0, "Number of curve points must be positive");
    for (int i = 0; i < count; i++) {
      lovrCurveAddPoint(curve, point, i);
    }
  } else {
    int pointIndex = 0;
    for (int i = 1; i <= top;) {
      float point[4];
      i = luax_readvec3(L, i, point, "vec3, number, or table");
      lovrCurveAddPoint(curve, point, pointIndex++);
    }
  }

  luax_pushtype(L, Curve, curve);
  lovrRelease(curve, lovrCurveDestroy);
  return 1;
}

extern int l_lovrMat4Set(lua_State* L);
static int l_lovrMathNewMat4(lua_State* L) {
  luax_newmat4(L);
  lua_insert(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathNewRandomGenerator(lua_State* L) {
  RandomGenerator* generator = lovrRandomGeneratorCreate();
  if (lua_gettop(L) > 0){
    Seed seed = { .b64 = luax_checkrandomseed(L, 1) };
    lovrRandomGeneratorSetSeed(generator, seed);
  }
  luax_pushtype(L, RandomGenerator, generator);
  lovrRelease(generator, lovrRandomGeneratorDestroy);
  return 1;
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

extern int l_lovrRandomGeneratorRandom(lua_State* L);
static int l_lovrMathRandom(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandom(L);
}

extern int l_lovrRandomGeneratorRandomNormal(lua_State* L);
static int l_lovrMathRandomNormal(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandomNormal(L);
}

extern int l_lovrRandomGeneratorGetSeed(lua_State* L);
static int l_lovrMathGetRandomSeed(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorGetSeed(L);
}

extern int l_lovrRandomGeneratorSetSeed(lua_State* L);
static int l_lovrMathSetRandomSeed(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorSetSeed(L);
}

static int l_lovrMathGammaToLinear(lua_State* L) {
  if (lua_istable(L, 1)) {
    for (int i = 0; i < 3; i++) {
      lua_rawgeti(L, 1, i + 1);
      float component = luax_checkfloat(L, -1);
      lua_pop(L, 1);
      lua_pushnumber(L, lovrMathGammaToLinear(component));
    }
    return 3;
  } else {
    int n = CLAMP(lua_gettop(L), 1, 3);
    for (int i = 0; i < n; i++) {
      lua_pushnumber(L, lovrMathGammaToLinear(luax_checkfloat(L, i + 1)));
    }
    return n;
  }
}

static int l_lovrMathLinearToGamma(lua_State* L) {
  if (lua_istable(L, 1)) {
    for (int i = 0; i < 3; i++) {
      lua_rawgeti(L, 1, i + 1);
      float component = luax_checkfloat(L, -1);
      lua_pop(L, 1);
      lua_pushnumber(L, lovrMathLinearToGamma(component));
    }
    return 3;
  } else {
    int n = CLAMP(lua_gettop(L), 1, 3);
    for (int i = 0; i < n; i++) {
      lua_pushnumber(L, lovrMathLinearToGamma(luax_checkfloat(L, i + 1)));
    }
    return n;
  }
}

static const luaL_Reg lovrMath[] = {
  { "newCurve", l_lovrMathNewCurve },
  { "newMat4", l_lovrMathNewMat4 },
  { "mat4", l_lovrMathNewMat4 },
  { "newRandomGenerator", l_lovrMathNewRandomGenerator },
  { "noise", l_lovrMathNoise },
  { "random", l_lovrMathRandom },
  { "randomNormal", l_lovrMathRandomNormal },
  { "getRandomSeed", l_lovrMathGetRandomSeed },
  { "setRandomSeed", l_lovrMathSetRandomSeed },
  { "gammaToLinear", l_lovrMathGammaToLinear },
  { "linearToGamma", l_lovrMathLinearToGamma },
  { NULL, NULL }
};

extern const luaL_Reg lovrCurve[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrMat4[];

int luaopen_lovr_math(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrMath);
  luax_registertype(L, Curve);
  luax_registertype(L, RandomGenerator);

  luaL_newmetatable(L, "Mat4");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luax_register(L, lovrMat4);
  lua_pop(L, 1);

  // Module
  lovrMathInit();
  luax_atexit(L, lovrMathDestroy);

  const char* names[] = { "vec2", "vec3", "vec4", "quat", "mat4" };
  const char* capitals[] = { "newVec2", "newVec3", "newVec4", "newQuat", "newMat4" };

  // Lua vectors
  if (!luaL_loadbuffer(L, (const char*) src_api_l_math_lua, src_api_l_math_lua_len, "@math.lua")) {
    luaL_newmetatable(L, "Vec2");
    luaL_newmetatable(L, "Vec3");
    luaL_newmetatable(L, "Vec4");
    luaL_newmetatable(L, "Quat");
    luaL_newmetatable(L, "Mat4");
    lua_call(L, 5, 0);
  } else {
    lovrThrow("%s", lua_tostring(L, -1));
    lua_pop(L, 1);
  }

  for (size_t i = 0; i < COUNTOF(names) - 1; i++) {
    luaL_newmetatable(L, capitals[i] + 3);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, names[i]); // lovr.math.vec3 = metatable
    lua_setfield(L, -2, capitals[i]); // lovr.math.newVec3 = metatable
  }

  // Globals
  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "math");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "globals");
      if (lua_toboolean(L, -1)) {
        for (size_t i = 0; i < COUNTOF(names); i++) {
          lua_getfield(L, -4, names[i]);
          lua_pushvalue(L, -1);
          lua_setglobal(L, names[i]);
          lua_setglobal(L, capitals[i] + 3); // Skip 'new'
        }
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  return 1;
}
