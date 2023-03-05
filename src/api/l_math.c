#include "api.h"
#include "math/math.h"
#include "data/image.h"
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
extern const luaL_Reg lovrLightProbe[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrVec2[];
extern const luaL_Reg lovrVec3[];
extern const luaL_Reg lovrVec4[];
extern const luaL_Reg lovrQuat[];
extern const luaL_Reg lovrMat4[];

static LOVR_THREAD_LOCAL Pool* pool;

static struct { const char* name; lua_CFunction constructor, indexer; const luaL_Reg* api; int metaref; } lovrVectorInfo[] = {
  [V_VEC2] = { "vec2", l_lovrMathVec2, l_lovrVec2__metaindex, lovrVec2, LUA_REFNIL },
  [V_VEC3] = { "vec3", l_lovrMathVec3, l_lovrVec3__metaindex, lovrVec3, LUA_REFNIL },
  [V_VEC4] = { "vec4", l_lovrMathVec4, l_lovrVec4__metaindex, lovrVec4, LUA_REFNIL },
  [V_QUAT] = { "quat", l_lovrMathQuat, l_lovrQuat__metaindex, lovrQuat, LUA_REFNIL },
  [V_MAT4] = { "mat4", l_lovrMathMat4, l_lovrMat4__metaindex, lovrMat4, LUA_REFNIL }
};

static void luax_destroypool(void) {
  lovrRelease(pool, lovrPoolDestroy);
}

float* luax_tovector(lua_State* L, int index, VectorType* type) {
  void* p = lua_touserdata(L, index);

  if (p) {
    if (lua_type(L, index) == LUA_TLIGHTUSERDATA) {
      Vector v = { .pointer = p };
      if (v.handle.type > V_NONE && v.handle.type < MAX_VECTOR_TYPES) {
        *type = v.handle.type;
        return lovrPoolResolve(pool, v);
      }
    } else {
      VectorType* t = p;
      if (*t > V_NONE && *t < MAX_VECTOR_TYPES) {
        *type = *t;
        return (float*) (t + 1);
      }
    }
  }

  *type = V_NONE;
  return NULL;
}

float* luax_checkvector(lua_State* L, int index, VectorType type, const char* expected) {
  VectorType t;
  float* p = luax_tovector(L, index, &t);
  if (!p || t != type) luax_typeerror(L, index, expected ? expected : lovrVectorInfo[type].name);
  return p;
}

static float* luax_newvector(lua_State* L, VectorType type, size_t components) {
  VectorType* p = lua_newuserdata(L, sizeof(VectorType) + components * sizeof(float));
  *p = type;
  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorInfo[type].metaref);
  lua_setmetatable(L, -2);
  return (float*) (p + 1);
}

float* luax_newtempvector(lua_State* L, VectorType type) {
  float* data;
  Vector vector = lovrPoolAllocate(pool, type, &data);
  lua_pushlightuserdata(L, vector.pointer);
  return data;
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
    float point[4] = { 0 };
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

static void getPixelEquirect(void* image, uint32_t x, uint32_t y, uint32_t z, float color[4]) {
  lovrImageGetPixel(image, x, y, z, color);
  if (lovrImageIsSRGB(image)) {
    color[0] = lovrMathGammaToLinear(color[0]);
    color[1] = lovrMathGammaToLinear(color[1]);
    color[2] = lovrMathGammaToLinear(color[2]);
  }
}

static void getPixelCubemap(void* image, uint32_t x, uint32_t y, uint32_t z, float color[4]) {
  lovrImageGetPixel(image, x, y, z, color);
  if (lovrImageIsSRGB(image)) {
    color[0] = lovrMathGammaToLinear(color[0]);
    color[1] = lovrMathGammaToLinear(color[1]);
    color[2] = lovrMathGammaToLinear(color[2]);
  }
}

static void getPixelCubemapLayers(void* context, uint32_t x, uint32_t y, uint32_t z, float color[4]) {
  Image** images = context;
  lovrImageGetPixel(images[z], x, y, 0, color);
  if (lovrImageIsSRGB(images[z])) {
    color[0] = lovrMathGammaToLinear(color[0]);
    color[1] = lovrMathGammaToLinear(color[1]);
    color[2] = lovrMathGammaToLinear(color[2]);
  }
}

static int l_lovrMathNewLightProbe(lua_State* L) {
  LightProbe* probe = lovrLightProbeCreate();
  if (lua_istable(L, 1)) {
    switch (luax_len(L, 1)) {
      case 9: {
        float coefficients[9][3], color[4];
        for (int i = 0; i < 9; i++) {
          lua_rawgeti(L, 1, i + 1);
          luax_optcolor(L, -1, color);
          memcpy(coefficients[i], color, 3 * sizeof(float));
          lua_pop(L, 1);
        }
        lovrLightProbeSetCoefficients(probe, coefficients);
        break;
      }
      case 6: {
        Image* images[6];

        for (int i = 0; i < 6; i++) {
          lua_rawgeti(L, 1, i + 1);
          images[i] = luax_checkimage(L, -1);
          lua_pop(L, 1);
        }

        uint32_t width = lovrImageGetWidth(images[0], 0);
        uint32_t height = lovrImageGetHeight(images[0], 0);
        lovrCheck(width == height, "Cubemap images must be square");

        for (uint32_t i = 0; i < 6; i++) {
          lovrCheck(lovrImageGetWidth(images[i], 0) == width, "Cubemap face images must have the same dimensions");
          lovrCheck(lovrImageGetHeight(images[i], 0) == height, "Cubemap face images must have the same dimensions");
          lovrCheck(lovrImageGetLayerCount(images[i]) == 1, "Cubemap face images all need to have a single layer");
        }

        lovrLightProbeAddCubemap(probe, width, getPixelCubemapLayers, images);

        for (uint32_t i = 0; i < 6; i++) {
          lovrRelease(images[i], lovrImageDestroy);
        }
        break;
      }
      default: lovrThrow("Expected a table with 9 colors or 6 images");
    }
  } else if (!lua_isnoneornil(L, 1)) {
    LightProbe* other = luax_totype(L, 1, LightProbe);

    if (other) {
      lovrLightProbeAddProbe(probe, other);
    } else {
      Image* image = luax_checkimage(L, 1);

      if (!image) {
        return luax_typeerror(L, 1, "table, LightProbe, string, Blob, or Image");
      }

      uint32_t width = lovrImageGetWidth(image, 0);
      uint32_t height = lovrImageGetHeight(image, 0);
      uint32_t layers = lovrImageGetLayerCount(image);

      if (layers == 1) {
        lovrCheck(width == 2 * height, "Equirectangular image width must be twice its height");
        lovrLightProbeAddEquirect(probe, width, height, getPixelEquirect, image);
        lovrRelease(image, lovrImageDestroy);
      } else if (layers == 6) {
        lovrCheck(width == height, "Cubemap images must be square");
        lovrLightProbeAddCubemap(probe, width, getPixelCubemap, image);
        lovrRelease(image, lovrImageDestroy);
      } else {
        lovrRelease(image, lovrImageDestroy);
        lovrThrow("Image layer count must be 1 or 6");
      }
    }
  }
  luax_pushtype(L, LightProbe, probe);
  lovrRelease(probe, lovrLightProbeDestroy);
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
  lua_replace(L, 1);
  return l_lovrVec2Set(L);
}

static int l_lovrMathVec3(lua_State* L) {
  luax_newtempvector(L, V_VEC3);
  lua_replace(L, 1);
  return l_lovrVec3Set(L);
}

static int l_lovrMathVec4(lua_State* L) {
  luax_newtempvector(L, V_VEC4);
  lua_replace(L, 1);
  return l_lovrVec4Set(L);
}

static int l_lovrMathQuat(lua_State* L) {
  luax_newtempvector(L, V_QUAT);
  lua_replace(L, 1);
  return l_lovrQuatSet(L);
}

static int l_lovrMathMat4(lua_State* L) {
  luax_newtempvector(L, V_MAT4);
  lua_replace(L, 1);
  return l_lovrMat4Set(L);
}

static int l_lovrMathDrain(lua_State* L) {
  lovrPoolDrain(pool);
  return 0;
}

static const luaL_Reg lovrMath[] = {
  { "newCurve", l_lovrMathNewCurve },
  { "newLightProbe", l_lovrMathNewLightProbe },
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
  { "drain", l_lovrMathDrain },
  { NULL, NULL }
};

static int l_lovrLightUserdata__index(lua_State* L) {
  VectorType type;
  if (!luax_tovector(L, 1, &type)) {
    return 0;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorInfo[type].metaref);
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, lovrVectorInfo[type].metaref);
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
  luax_register(L, lovrMath);
  luax_registertype(L, Curve);
  luax_registertype(L, LightProbe);
  luax_registertype(L, RandomGenerator);

  for (size_t i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
    lua_newtable(L);

    lua_newtable(L);
    lua_pushcfunction(L, lovrVectorInfo[i].constructor);
    lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, lovrVectorInfo[i].indexer);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    lua_pushstring(L, lovrVectorInfo[i].name);
    lua_setfield(L, -2, "__name");

    // lovr.math[__name] = metatable
    lua_pushstring(L, lovrVectorInfo[i].name);
    lua_pushvalue(L, -2);
    lua_settable(L, -4);

    luax_register(L, lovrVectorInfo[i].api);
    lovrVectorInfo[i].metaref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  // Global lightuserdata metatable
  lua_pushlightuserdata(L, NULL);
  lua_newtable(L);
  lua_pushcfunction(L, l_lovrLightUserdata__index);
  lua_setfield(L, -2, "__index");
  const char* ops[] = { "__add", "__sub", "__mul", "__div", "__unm", "__len", "__tostring", "__newindex" };
  for (size_t i = 0; i < COUNTOF(ops); i++) {
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

  // Each Lua state gets its own thread-local Pool
  pool = lovrPoolCreate();
  luax_atexit(L, luax_destroypool);

  // Globals
  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "math");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "globals");
      if (lua_toboolean(L, -1)) {
        for (size_t i = V_NONE + 1; i < MAX_VECTOR_TYPES; i++) {
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
