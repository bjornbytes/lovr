#include "api.h"
#include "math/math.h"
#include "math/curve.h"
#include "math/pool.h"
#include "math/randomGenerator.h"
#include "resources/math.lua.h"
#include "core/ref.h"
#include <stdlib.h>

int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);
int l_lovrVec3Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);

static const char* lovrMathTypeNames[] = {
  [MATH_VEC3] = "vec3",
  [MATH_QUAT] = "quat",
  [MATH_MAT4] = "mat4"
};

static const luaL_Reg* lovrMathTypes[] = {
  [MATH_VEC3] = lovrVec3,
  [MATH_QUAT] = lovrQuat,
  [MATH_MAT4] = lovrMat4
};

static const int lovrMathTypeComponents[] = {
  [MATH_VEC3] = 3,
  [MATH_QUAT] = 4,
  [MATH_MAT4] = 16
};

static float* luax_tolightmathtype(lua_State* L, int index, MathType* type) {
  uintptr_t p = (uintptr_t) lua_touserdata(L, index), aligned = ALIGN(p, POOL_ALIGN);
  return *type = p - aligned, p ? (float*) aligned : NULL;
}

void luax_pushlightmathtype(lua_State* L, float* p, MathType type) {
  lua_pushlightuserdata(L, (uint8_t*) p + type);
}

float* luax_tomathtype(lua_State* L, int index, MathType* type) {
  int luaType = lua_type(L, index);
  if (luaType == LUA_TLIGHTUSERDATA) {
    return luax_tolightmathtype(L, index, type);
  } else if (luaType == LUA_TUSERDATA) {
    MathType* x = lua_touserdata(L, index);
    *type = *x;
    return (float*) (x + 1);
  } else if (luaType > LUA_TTHREAD) { // cdata
    lua_getfield(L, index, "_type");
    *type = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "_p");
    float* p = *(float**) lua_topointer(L, -1);
    lua_pop(L, 1);
    return p;
  }
  return NULL;
}

float* luax_checkmathtype(lua_State* L, int index, MathType type, const char* expected) {
  MathType t;
  float* p = luax_tomathtype(L, index, &t);
  if (!p || t != type) luaL_typerror(L, index, expected ? expected : lovrMathTypeNames[type]);
  return p;
}

float* luax_newmathtype(lua_State* L, MathType type) {
  MathType* x = (MathType*) lua_newuserdata(L, sizeof(MathType) + lovrMathTypeComponents[type] * sizeof(float));
  luaL_getmetatable(L, lovrMathTypeNames[type]);
  lua_setmetatable(L, -2);
  *x = type;
  return (float*) (x + 1);
}

static int l_lovrMathNewCurve(lua_State* L) {
  int top = lua_gettop(L);

  if (top == 1 && lua_type(L, 1) == LUA_TNUMBER) {
    Curve* curve = lovrCurveCreate(luaL_checkinteger(L, 1));
    luax_pushtype(L, Curve, curve);
    return 1;
  }

  Curve* curve = lovrCurveCreate(3);

  if (lua_istable(L, 1)) {
    int pointIndex = 0;
    int length = luax_len(L, 1);
    for (int i = 1; i <= length;) {
      lua_rawgeti(L, 1, i + 0);
      lua_rawgeti(L, 1, i + 1);
      lua_rawgeti(L, 1, i + 2);
      float point[3];
      int components = luax_readvec3(L, -3, point, "vec3 or number");
      lovrCurveAddPoint(curve, point, pointIndex++);
      i += 3 + components;
      lua_pop(L, 3);
    }
  } else {
    int pointIndex = 0;
    for (int i = 1; i <= top;) {
      float point[3];
      i = luax_readvec3(L, i, point, "vec3, number, or table");
      lovrCurveAddPoint(curve, point, pointIndex++);
    }
  }

  luax_pushtype(L, Curve, curve);
  lovrRelease(Curve, curve);
  return 1;
}

static int l_lovrMathNewPool(lua_State* L) {
  size_t size = luaL_optinteger(L, 1, DEFAULT_POOL_SIZE);
  Pool* pool = lovrPoolCreate(size);
  luax_pushtype(L, Pool, pool);
  lovrRelease(Pool, pool);
  return 1;
}

static int l_lovrMathNewRandomGenerator(lua_State* L) {
  RandomGenerator* generator = lovrRandomGeneratorCreate();
  if (lua_gettop(L) > 0){
    Seed seed = luax_checkrandomseed(L, 1);
    lovrRandomGeneratorSetSeed(generator, seed);
  }
  luax_pushtype(L, RandomGenerator, generator);
  lovrRelease(RandomGenerator, generator);
  return 1;
}

static int l_lovrMathLookAt(lua_State* L) {
  float from[3] = { luax_checkfloat(L, 1), luax_checkfloat(L, 2), luax_checkfloat(L, 3) };
  float to[3] = { luax_checkfloat(L, 4), luax_checkfloat(L, 5), luax_checkfloat(L, 6) };
  float up[3] = { luax_optfloat(L, 7, 0.f), luax_optfloat(L, 8, 1.f), luax_optfloat(L, 9, 0.f) };
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
  float angle = luax_checkfloat(L, 1);
  float ax = luax_optfloat(L, 2, 0.f);
  float ay = luax_optfloat(L, 3, 1.f);
  float az = luax_optfloat(L, 4, 0.f);
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
    case 1: lua_pushnumber(L, lovrMathNoise1(luax_checkfloat(L, 1))); return 1;
    case 2: lua_pushnumber(L, lovrMathNoise2(luax_checkfloat(L, 1), luax_checkfloat(L, 2))); return 1;
    case 3: lua_pushnumber(L, lovrMathNoise3(luax_checkfloat(L, 1), luax_checkfloat(L, 2), luax_checkfloat(L, 3))); return 1;
    case 4:
    default:
      lua_pushnumber(L, lovrMathNoise4(luax_checkfloat(L, 1), luax_checkfloat(L, 2), luax_checkfloat(L, 3), luax_checkfloat(L, 4)));
      return 1;
  }
}

static int l_lovrMathRandom(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandom(L);
}

static int l_lovrMathRandomNormal(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorRandomNormal(L);
}

static int l_lovrMathGetRandomSeed(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorGetSeed(L);
}

static int l_lovrMathSetRandomSeed(lua_State* L) {
  luax_pushtype(L, RandomGenerator, lovrMathGetRandomGenerator());
  lua_insert(L, 1);
  return l_lovrRandomGeneratorSetSeed(L);
}

static int l_lovrMathGammaToLinear(lua_State* L) {
  if (lua_istable(L, 1)) {
    for (int i = 0; i < 3; i++) {
      lua_rawgeti(L, 1, i + 1);
      lua_pushnumber(L, lovrMathGammaToLinear(luax_checkfloat(L, -1)));
    }
    lua_pop(L, 3);
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
      lua_pushnumber(L, lovrMathLinearToGamma(luax_checkfloat(L, -1)));
    }
    lua_pop(L, 3);
    return 3;
  } else {
    int n = CLAMP(lua_gettop(L), 1, 3);
    for (int i = 0; i < n; i++) {
      lua_pushnumber(L, lovrMathLinearToGamma(luax_checkfloat(L, i + 1)));
    }
    return n;
  }
}

static int l_lovrVec3__call(lua_State* L) {
  luax_newmathtype(L, MATH_VEC3);
  lua_replace(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrQuat__call(lua_State* L) {
  luax_newmathtype(L, MATH_QUAT);
  lua_replace(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMat4__call(lua_State* L) {
  luax_newmathtype(L, MATH_MAT4);
  lua_replace(L, 1);
  return l_lovrMat4Set(L);
}

static const luaL_Reg lovrMath[] = {
  { "newCurve", l_lovrMathNewCurve },
  { "newPool", l_lovrMathNewPool },
  { "newRandomGenerator", l_lovrMathNewRandomGenerator },
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

static int l_lovrLightUserdata__index(lua_State* L) {
  MathType type;
  luax_tolightmathtype(L, 1, &type);
  luaL_getmetatable(L, lovrMathTypeNames[type]);
  lua_pushvalue(L, 2);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrLightUserdataOp(lua_State* L) {
  MathType type;
  luax_tolightmathtype(L, 1, &type);
  luaL_getmetatable(L, lovrMathTypeNames[type]);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_gettable(L, -2);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_call(L, 2, 1);
  return 1;
}

int luaopen_lovr_math(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, Curve);
  luax_registertype(L, Pool);
  luax_registertype(L, RandomGenerator);

  for (int i = 0; i < MAX_MATH_TYPES; i++) {
    _luax_registertype(L, lovrMathTypeNames[i], lovrMathTypes[i], NULL);
    luaL_getmetatable(L, lovrMathTypeNames[i]);

    // Remove usual __gc handler
    lua_pushnil(L);
    lua_setfield(L, -2, "__gc");

    // Allow metatable to be called as a function
    lua_newtable(L);
    switch (i) {
      case MATH_VEC3: lua_pushcfunction(L, l_lovrVec3__call); break;
      case MATH_QUAT: lua_pushcfunction(L, l_lovrQuat__call); break;
      case MATH_MAT4: lua_pushcfunction(L, l_lovrMat4__call); break;
      default: break;
    }
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    // Assign into the lovr.math table
    lua_setfield(L, -2, lovrMathTypeNames[i]);
  }

  // Global lightuserdata metatable
  lua_pushlightuserdata(L, NULL);
  lua_newtable(L);

  lua_pushcfunction(L, l_lovrLightUserdata__index);
  lua_setfield(L, -2, "__index");

  const char* ops[] = { "__add", "__sub", "__mul", "__div", "__unm", "__len", "__tostring" };
  for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
    lua_pushstring(L, ops[i]);
    lua_pushcclosure(L, l_lovrLightUserdataOp, 1);
    lua_setfield(L, -2, ops[i]);
  }

  lua_setmetatable(L, -2);
  lua_pop(L, 1);

  // Module
  if (lovrMathInit()) {
    luax_atexit(L, lovrMathDestroy);
  }

  // Inject LuaJIT superjuice
#ifdef LOVR_USE_LUAJIT
  lua_pushcfunction(L, luax_getstack);
  lovrAssert(!luaL_loadbuffer(L, (const char*) math_lua, math_lua_len, "math.lua"), "Could not load math.lua");
  lua_pushvalue(L, -3); // lovr.math
  luaL_getmetatable(L, "Pool");
  if (lua_pcall(L, 2, 0, -4)) {
    lovrThrow(lua_tostring(L, -1));
  }
  lua_pop(L, 1);
#endif

  return 1;
}
