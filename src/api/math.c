#include "api.h"
#include "api/math.h"
#include "api/math.lua.h"
#include "math/math.h"
#include "math/curve.h"
#include "math/pool.h"
#include "math/randomGenerator.h"

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

static int lovrMathTypeRefs[MAX_MATH_TYPES];

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
  } else if (luaType == LUA_TUSERDATA && lua_getmetatable(L, index)) {
    lua_pushliteral(L, "_type");
    lua_rawget(L, -2);
    *type = lua_tointeger(L, -1);
    lua_pop(L, 2);
    return lua_touserdata(L, index);
  } else if (luaType > LUA_TTHREAD) { // cdata
    lua_getfield(L, index, "_type");
    *type = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "_p");
    float* p = *(float**) lua_topointer(L, index);
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
  lovrRelease(curve);
  return 1;
}

static int l_lovrMathNewPool(lua_State* L) {
  size_t size = luaL_optinteger(L, 1, DEFAULT_POOL_SIZE);
  Pool* pool = lovrPoolCreate(size);
  luax_pushobject(L, pool);
  lovrRelease(pool);
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

static int l_lovrMathVec3(lua_State* L) {
  luax_pushobject(L, lovrMathGetPool());
  lua_insert(L, 1);
  return l_lovrPoolVec3(L);
}

static int l_lovrMathQuat(lua_State* L) {
  luax_pushobject(L, lovrMathGetPool());
  lua_insert(L, 1);
  return l_lovrPoolQuat(L);
}

static int l_lovrMathMat4(lua_State* L) {
  luax_pushobject(L, lovrMathGetPool());
  lua_insert(L, 1);
  return l_lovrPoolMat4(L);
}

static int l_lovrMathDrain(lua_State* L) {
  luax_pushobject(L, lovrMathGetPool());
  lua_insert(L, 1);
  return l_lovrPoolDrain(L);
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
  { "vec3", l_lovrMathVec3 },
  { "quat", l_lovrMathQuat },
  { "mat4", l_lovrMathMat4 },
  { "drain", l_lovrMathDrain },
  { NULL, NULL }
};

static int l_lovrLightUserdata__index(lua_State* L) {
  MathType type;
  luax_tolightmathtype(L, 1, &type);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrMathTypeRefs[type]);
  lua_pushvalue(L, 2);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrLightUserdataOp(lua_State* L) {
  MathType type;
  luax_tolightmathtype(L, 1, &type);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrMathTypeRefs[type]);
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
  luax_registertype(L, "Curve", lovrCurve);
  luax_registertype(L, "Pool", lovrPool);
  luax_registertype(L, "RandomGenerator", lovrRandomGenerator);

  // Store every math type metatable in the registry and register it as a type
  for (int i = 0; i < MAX_MATH_TYPES; i++) {
    lua_newtable(L);
    luaL_register(L, NULL, lovrMathTypes[i]);
    lovrMathTypeRefs[i] = luaL_ref(L, LUA_REGISTRYINDEX);

    luaL_newmetatable(L, lovrMathTypeNames[i]);
    lua_pushvalue(L, -1);
    lua_setfield(L, -1, "__index");
    lua_pushinteger(L, i);
    lua_setfield(L, -2, "_type");
    luaL_register(L, NULL, lovrMathTypes[i]);
    lua_pop(L, 1);
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

  // Pool size
  luax_pushconf(L);
  lua_getfield(L, -1, "math");
  size_t poolSize = DEFAULT_POOL_SIZE;
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "poolsize");
    if (lua_isnumber(L, -1)) {
      poolSize = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  // Module
  if (lovrMathInit(poolSize)) {
    luax_atexit(L, lovrMathDestroy);
  }

  // Inject LuaJIT superjuice
  lua_pushcfunction(L, luax_getstack);
  lovrAssert(!luaL_loadbuffer(L, (const char*) math_lua, math_lua_len, "math.lua"), "Could not load math.lua");
  lua_pushvalue(L, -3); // lovr.math
  luaL_getmetatable(L, "Pool");
  if (lua_pcall(L, 2, 0, -4)) {
    lovrThrow(lua_tostring(L, -1));
  }
  lua_pop(L, 1);

  return 1;
}
