#include "api.h"
#include "math/randomGenerator.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <math.h>

static double luax_checkrandomseedpart(lua_State* L, int index) {
  double x = luaL_checknumber(L, index);

  if (!isfinite(x)) {
    luaL_argerror(L, index, "invalid random seed");
  }

  return x;
}

uint64_t luax_checkrandomseed(lua_State* L, int index) {
  Seed seed;

  if (lua_isnoneornil(L, index + 1)) {
    seed.b64 = luax_checkrandomseedpart(L, index);
  } else {
    seed.b32.lo = luax_checkrandomseedpart(L, index);
    seed.b32.hi = luax_checkrandomseedpart(L, index + 1);
  }

  return seed.b64;
}

int l_lovrRandomGeneratorGetSeed(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  Seed seed = lovrRandomGeneratorGetSeed(generator);
  lua_pushnumber(L, seed.b32.lo);
  lua_pushnumber(L, seed.b32.hi);
  return 2;
}

int l_lovrRandomGeneratorSetSeed(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  Seed seed = { .b64 = luax_checkrandomseed(L, 2) };
  lovrRandomGeneratorSetSeed(generator, seed);
  return 0;
}

static int l_lovrRandomGeneratorGetState(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  size_t length = 32;
  char state[32];
  lovrRandomGeneratorGetState(generator, state, length);
  lua_pushstring(L, state);
  return 1;
}

static int l_lovrRandomGeneratorSetState(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  const char* state = luaL_checklstring(L, 2, NULL);
  if (lovrRandomGeneratorSetState(generator, state)) {
    return luaL_error(L, "invalid random state %s", state);
  }
  return 0;
}

int l_lovrRandomGeneratorRandom(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  double r = lovrRandomGeneratorRandom(generator);

  if (lua_gettop(L) >= 3) {
    double lower = luaL_checknumber(L, 2);
    double upper = luaL_checknumber(L, 3);
    lua_pushnumber(L, floor(r * (upper - lower + 1)) + lower);
  } else if (lua_gettop(L) >= 2) {
    double upper = luaL_checknumber(L, 2);
    lua_pushnumber(L, floor(r * upper) + 1);
  } else {
    lua_pushnumber(L, r);
  }

  return 1;
}

int l_lovrRandomGeneratorRandomNormal(lua_State* L) {
  RandomGenerator* generator = luax_checktype(L, 1, RandomGenerator);
  float sigma = luax_optfloat(L, 2, 1.f);
  float mu = luax_optfloat(L, 3, 0.f);
  lua_pushnumber(L, mu + lovrRandomGeneratorRandomNormal(generator) * sigma);
  return 1;
}

const luaL_Reg lovrRandomGenerator[] = {
  { "getSeed", l_lovrRandomGeneratorGetSeed },
  { "setSeed", l_lovrRandomGeneratorSetSeed },
  { "getState", l_lovrRandomGeneratorGetState },
  { "setState", l_lovrRandomGeneratorSetState },
  { "random", l_lovrRandomGeneratorRandom },
  { "randomNormal", l_lovrRandomGeneratorRandomNormal },
  { NULL, NULL }
};
