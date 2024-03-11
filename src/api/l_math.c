#include "api.h"
#include "math/math.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);
int l_lovrVec2Set(lua_State* L);
int l_lovrVec3Set(lua_State* L);
int l_lovrVec4Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);
int l_lovrVec2__metaindex(lua_State* L);
int l_lovrVec3__metaindex(lua_State* L);
int l_lovrVec4__metaindex(lua_State* L);
int l_lovrQuat__metaindex(lua_State* L);
int l_lovrMat4__metaindex(lua_State* L);
static int l_lovrMathVec2(lua_State* L);
static int l_lovrMathVec3(lua_State* L);
static int l_lovrMathVec4(lua_State* L);
static int l_lovrMathQuat(lua_State* L);
static int l_lovrMathMat4(lua_State* L);
extern const luaL_Reg lovrCurve[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrVec2[];
extern const luaL_Reg lovrVec3[];
extern const luaL_Reg lovrVec4[];
extern const luaL_Reg lovrQuat[];
extern const luaL_Reg lovrMat4[];

#define VECTOR_STACK_MAX 16

typedef struct {
  size_t components;
  const char* name;
  lua_CFunction metacall;
  lua_CFunction metaindex;
  const luaL_Reg* api;
  int metatableRef;
  int poolRef;
  int poolCursor;
  int stackPointers[VECTOR_STACK_MAX];
} VectorInfo;

static uint32_t stackIndex;

static VectorInfo lovrVectorInfo[] = {
  [V_VEC2] = { 2, "vec2", l_lovrMathVec2, l_lovrVec2__metaindex, lovrVec2, LUA_REFNIL, LUA_REFNIL, 1, { 0 } },
  [V_VEC3] = { 3, "vec3", l_lovrMathVec3, l_lovrVec3__metaindex, lovrVec3, LUA_REFNIL, LUA_REFNIL, 1, { 0 } },
  [V_VEC4] = { 4, "vec4", l_lovrMathVec4, l_lovrVec4__metaindex, lovrVec4, LUA_REFNIL, LUA_REFNIL, 1, { 0 } },
  [V_QUAT] = { 4, "quat", l_lovrMathQuat, l_lovrQuat__metaindex, lovrQuat, LUA_REFNIL, LUA_REFNIL, 1, { 0 } },
  [V_MAT4] = { 16, "mat4", l_lovrMathMat4, l_lovrMat4__metaindex, lovrMat4, LUA_REFNIL, LUA_REFNIL, 1, { 0 } }
};

float* luax_tovector(lua_State* L, int index, int* type) {
  void* p = lua_touserdata(L, index);

  if (p) {
    int* t = p;
    if (*t > V_NONE && *t < MAX_VECTOR_TYPES) {
      if (type) *type = *t;
      return (float*) (t + 1);
    }
  }

  if (type) *type = V_NONE;
  return NULL;
}

float* luax_checkvector(lua_State* L, int index, int type, const char* expected) {
  int t;
  float* p = luax_tovector(L, index, &t);
  if (!p || t != type) luax_typeerror(L, index, expected ? expected : lovrVectorInfo[type].name);
  return p;
}

float* luax_newvector(lua_State* L, int type) {
  if (stackIndex > 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorInfo[type].poolRef);
    lua_rawgeti(L, -1, lovrVectorInfo[type].poolCursor);
    if (!lua_isnil(L, -1)) {
      lua_remove(L, -2);
      lovrVectorInfo[type].poolCursor++;
      return (float*) ((char*) lua_topointer(L, -1) + sizeof(int));
    } else {
      lua_pop(L, 1);
    }
  }

  void* p = lua_newuserdata(L, sizeof(int) + lovrVectorInfo[type].components * sizeof(float));
  *((int*) p) = type;
  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorInfo[type].metatableRef);
  lua_setmetatable(L, -2);

  if (stackIndex > 0) {
    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, lovrVectorInfo[type].poolCursor++);
    lua_remove(L, -2);
  }

  return (float*) ((char*) p + sizeof(int));
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
  } else if (top == 1 && lua_type(L, 1) == LUA_TNUMBER) {
    float point[4] = { 0.f };
    int count = lua_tonumber(L, 1);
    lovrAssert(count > 0, "Number of curve points must be positive");
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

static int l_lovrMathNewVec2(lua_State* L) {
  luax_newvector(L, V_VEC2);
  lua_insert(L, 1);
  return l_lovrVec2Set(L);
}

static int l_lovrMathNewVec3(lua_State* L) {
  luax_newvector(L, V_VEC3);
  lua_insert(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrMathNewVec4(lua_State* L) {
  luax_newvector(L, V_VEC4);
  lua_insert(L, 1);
  return l_lovrVec4Set(L);
}

static int l_lovrMathNewQuat(lua_State* L) {
  luax_newvector(L, V_QUAT);
  lua_insert(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMathNewMat4(lua_State* L) {
  luax_newvector(L, V_MAT4);
  lua_insert(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathVec2(lua_State* L) {
  luax_newvector(L, V_VEC2);
  lua_replace(L, 1);
  return l_lovrVec2Set(L);
}

static int l_lovrMathVec3(lua_State* L) {
  luax_newvector(L, V_VEC3);
  lua_replace(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrMathVec4(lua_State* L) {
  luax_newvector(L, V_VEC4);
  lua_replace(L, 1);
  return l_lovrVec4Set(L);
}

static int l_lovrMathQuat(lua_State* L) {
  luax_newvector(L, V_QUAT);
  lua_replace(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMathMat4(lua_State* L) {
  luax_newvector(L, V_MAT4);
  lua_replace(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathPush(lua_State* L) {
  for (int i = V_VEC2; i <= V_MAT4; i++) {
    lovrVectorInfo[i].stackPointers[stackIndex] = lovrVectorInfo[i].poolCursor;
  }
  lovrAssert(++stackIndex < VECTOR_STACK_MAX, "Vector stack overflow (more pushes than pops?)");
  return 0;
}

static int l_lovrMathPop(lua_State* L) {
  lovrAssert(--stackIndex >= 0, "Vector stack underflow (more pops than pushes?)");
  for (int i = V_VEC2; i <= V_MAT4; i++) {
    lovrVectorInfo[i].poolCursor = lovrVectorInfo[i].stackPointers[stackIndex];
  }
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
  { "push", l_lovrMathPush },
  { "pop", l_lovrMathPop },
  { NULL, NULL }
};

int luaopen_lovr_math(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrMath);
  luax_registertype(L, Curve);
  luax_registertype(L, RandomGenerator);

  for (int i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
    lua_newtable(L);

    lua_newtable(L);
    lua_pushcfunction(L, lovrVectorInfo[i].metacall);
    lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, lovrVectorInfo[i].metaindex);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    lua_pushstring(L, lovrVectorInfo[i].name);
    lua_setfield(L, -2, "__name");

    // lovr.math[__name] = metatable
    lua_pushstring(L, lovrVectorInfo[i].name);
    lua_pushvalue(L, -2);
    lua_settable(L, -4);

    luax_register(L, lovrVectorInfo[i].api);
    lovrVectorInfo[i].metatableRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // pool
    lua_newtable(L);

    // __mode = v
    lua_newtable(L);
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lovrVectorInfo[i].poolRef = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // Module
  lovrMathInit();
  luax_atexit(L, lovrMathDestroy);

  // Globals
  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "math");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "globals");
      if (lua_toboolean(L, -1)) {
        for (int i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
          lua_getfield(L, -4, lovrVectorInfo[i].name);
          lua_setglobal(L, lovrVectorInfo[i].name);

          // Capitalized global is permanent vector constructor
          char constructor[8];
          memcpy(constructor, "new", 3);
          memcpy(constructor + 3, lovrVectorInfo[i].name, 4);
          constructor[3] -= 32;
          constructor[7] = '\0';
          lua_getfield(L, -4, constructor);
          lua_setglobal(L, constructor + 3);
        }
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  return 1;
}
