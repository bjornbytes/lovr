#include "api.h"
#include "math/math.h"
#include "math/curve.h"
#include "math/pool.h"
#include "math/randomGenerator.h"
#include "resources/math.lua.h"
#include "core/maf.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>

int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);
int l_lovrVec2Set(lua_State* L);
int l_lovrVec3Set(lua_State* L);
int l_lovrVec4Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);

static LOVR_THREAD_LOCAL Pool* pool;

static const luaL_Reg* lovrVectorMetatables[] = {
  [V_VEC2] = lovrVec2,
  [V_VEC3] = lovrVec3,
  [V_VEC4] = lovrVec4,
  [V_QUAT] = lovrQuat,
  [V_MAT4] = lovrMat4
};

static int lovrVectorMetatableRefs[] = {
  [V_VEC2] = LUA_REFNIL,
  [V_VEC3] = LUA_REFNIL,
  [V_VEC4] = LUA_REFNIL,
  [V_QUAT] = LUA_REFNIL,
  [V_MAT4] = LUA_REFNIL
};

static const char* lovrVectorTypeNames[] = {
  [V_VEC2] = "vec2",
  [V_VEC3] = "vec3",
  [V_VEC4] = "vec4",
  [V_QUAT] = "quat",
  [V_MAT4] = "mat4"
};

static void luax_destroypool(void) {
  lovrRelease(Pool, pool);
}

float* luax_tovector(lua_State* L, int index, VectorType* type) {
  int luaType = lua_type(L, index);
  if (luaType == LUA_TLIGHTUSERDATA) {
    union { void* pointer; Vector handle; } u;
    u.pointer = lua_touserdata(L, index);
    if (u.pointer && u.handle.type > V_NONE && u.handle.type < MAX_VECTOR_TYPES) {
      *type = u.handle.type;
      return lovrPoolResolve(pool, u.handle);
    }
  } else if (luaType > LUA_TTHREAD) { // cdata
    Vector* handle = (Vector*) lua_topointer(L, index);
    if (handle && handle->type > V_NONE && handle->type < MAX_VECTOR_TYPES) {
      *type = handle->type;
      return lovrPoolResolve(pool, *handle);
    }
  } else if (luaType == LUA_TUSERDATA) {
    VectorType* p = lua_touserdata(L, index);
    if (p && *p > V_NONE && *p < MAX_VECTOR_TYPES) {
      *type = *p;
      return (float*) (p + 1);
    }
  }
  *type = V_NONE;
  return NULL;
}

float* luax_checkvector(lua_State* L, int index, VectorType type, const char* expected) {
  VectorType t;
  float* p = luax_tovector(L, index, &t);
  if (!p || t != type) luaL_typerror(L, index, expected ? expected : lovrVectorTypeNames[type]);
  return p;
}

static float* luax_newvector(lua_State* L, VectorType type, size_t components) {
  VectorType* p = lua_newuserdata(L, sizeof(VectorType) + components * sizeof(float));
  *p = type;
  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorMetatableRefs[type]);
  lua_setmetatable(L, -2);
  return (float*) (p + 1);
}

float* luax_newtempvector(lua_State* L, VectorType type) {
  float* data;
  Vector handle = lovrPoolAllocate(pool, type, &data);
  union { void* pointer; Vector handle; } u;
  u.handle = handle;
  lua_pushlightuserdata(L, u.pointer);
  return data;
}

static int l_lovrVectorGetPointer(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    VectorType* p = lua_touserdata(L, 1);
    lua_pushlightuserdata(L, p + 1);
    return 1;
  }
  return 0;
}

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
  } else {
    int pointIndex = 0;
    for (int i = 1; i <= top;) {
      float point[4];
      i = luax_readvec3(L, i, point, "vec3, number, or table");
      lovrCurveAddPoint(curve, point, pointIndex++);
    }
  }

  luax_pushtype(L, Curve, curve);
  lovrRelease(Curve, curve);
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

static int l_lovrMathNewVec2(lua_State* L) {
  luax_newvector(L, V_VEC2, 2);
  lua_insert(L, 1);
  return l_lovrVec2Set(L);
}

static int l_lovrMathNewVec3(lua_State* L) {
  luax_newvector(L, V_VEC3, 4);
  lua_insert(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrMathNewVec4(lua_State* L) {
  luax_newvector(L, V_VEC4, 4);
  lua_insert(L, 1);
  return l_lovrVec4Set(L);
}

static int l_lovrMathNewQuat(lua_State* L) {
  luax_newvector(L, V_QUAT, 4);
  lua_insert(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMathNewMat4(lua_State* L) {
  luax_newvector(L, V_MAT4, 16);
  lua_insert(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathVec2(lua_State* L) {
  luax_newtempvector(L, V_VEC2);
  lua_insert(L, 1);
  return l_lovrVec2Set(L);
}

static int l_lovrMathVec3(lua_State* L) {
  luax_newtempvector(L, V_VEC3);
  lua_insert(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrMathVec4(lua_State* L) {
  luax_newtempvector(L, V_VEC4);
  lua_insert(L, 1);
  return l_lovrVec4Set(L);
}

static int l_lovrMathQuat(lua_State* L) {
  luax_newtempvector(L, V_QUAT);
  lua_insert(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMathMat4(lua_State* L) {
  luax_newtempvector(L, V_MAT4);
  lua_insert(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathDrain(lua_State* L) {
  lovrPoolDrain(pool);
  return 0;
}

static const luaL_Reg lovrMath[] = {
  { "newCurve", l_lovrMathNewCurve },
  { "newRandomGenerator", l_lovrMathNewRandomGenerator },
  { "noise", l_lovrMathNoise },
  { "random", l_lovrMathRandom },
  { "randomNormal", l_lovrMathRandomNormal },
  { "getRandomSeed", l_lovrMathGetRandomSeed },
  { "setRandomSeed", l_lovrMathSetRandomSeed },
  { "gammaToLinear", l_lovrMathGammaToLinear },
  { "linearToGamma", l_lovrMathLinearToGamma },
  { "newVec2", l_lovrMathNewVec2 },
  { "newVec3", l_lovrMathNewVec3 },
  { "newVec4", l_lovrMathNewVec4 },
  { "newQuat", l_lovrMathNewQuat },
  { "newMat4", l_lovrMathNewMat4 },
  { "vec2", l_lovrMathVec2 },
  { "vec3", l_lovrMathVec3 },
  { "vec4", l_lovrMathVec4 },
  { "quat", l_lovrMathQuat },
  { "mat4", l_lovrMathMat4 },
  { "drain", l_lovrMathDrain },
  { NULL, NULL }
};

static int l_lovrLightUserdata__index(lua_State* L) {
  VectorType type;
  if (!luax_tovector(L, 1, &type)) {
    return 0;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorMetatableRefs[type]);
  lua_pushvalue(L, 2);
  lua_rawget(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_pushliteral(L, "__index");
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      lua_pushvalue(L, 1);
      lua_pushvalue(L, 2);
      lua_call(L, 2, 1);
      return 1;
    } else {
      return 0;
    }
  }
  return 1;
}

static int l_lovrLightUserdataOp(lua_State* L) {
  VectorType type;
  if (!luax_tovector(L, 1, &type)) {
    lua_pushliteral(L, "__tostring");
    if (lua_rawequal(L, -1, lua_upvalueindex(1))) {
      lua_pop(L, 1);
      lua_pushfstring(L, "%s: %p", lua_typename(L, lua_type(L, 1)), lua_topointer(L, 1));
      return 1;
    }
    lua_pop(L, 1);
    return luaL_error(L, "Unsupported lightuserdata operator %q", lua_tostring(L, lua_upvalueindex(1)));
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorMetatableRefs[type]);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_gettable(L, -2);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_pushvalue(L, 3);
  lua_call(L, 3, 1);
  return 1;
}

int luaopen_lovr_math(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, Curve);
  luax_registertype(L, RandomGenerator);

  for (size_t i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
    lua_newtable(L);
    lua_pushstring(L, lovrVectorTypeNames[i]);
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, l_lovrVectorGetPointer);
    lua_setfield(L, -2, "getPointer");
    luaL_register(L, NULL, lovrVectorMetatables[i]);
    lovrVectorMetatableRefs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // Global lightuserdata metatable
  lua_pushlightuserdata(L, NULL);
  lua_newtable(L);
  lua_pushcfunction(L, l_lovrLightUserdata__index);
  lua_setfield(L, -2, "__index");
  const char* ops[] = { "__add", "__sub", "__mul", "__div", "__unm", "__len", "__tostring", "__newindex" };
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

  pool = lovrPoolCreate();
  luax_atexit(L, luax_destroypool);

  // Inject LuaJIT superjuice
  luax_pushconf(L);
  lua_getfield(L, -1, "math");
  if (lua_istable(L, -1)) {
#ifdef LOVR_USE_LUAJIT
    lua_getfield(L, -1, "ffi");
    if (lua_toboolean(L, -1)) {
      lua_pushcfunction(L, luax_getstack);
      if (luaL_loadbuffer(L, (const char*) math_lua, math_lua_len, "math.lua")) {
        lovrThrow(lua_tostring(L, -1));
      }
      lua_pushvalue(L, -6); // lovr.math
      lua_pushlightuserdata(L, pool);
      if (lua_pcall(L, 2, 0, -4)) {
        lovrThrow(lua_tostring(L, -1));
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
#endif
    lua_getfield(L, -1, "globals");
    if (lua_toboolean(L, -1)) {
      for (size_t i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
        lua_getfield(L, -4, lovrVectorTypeNames[i]);
        lua_setglobal(L, lovrVectorTypeNames[i]);
      }
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  return 1;
}
