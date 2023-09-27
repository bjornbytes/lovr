#include "api.h"
#include "util.h"
#include <string.h>

static int l_lovrSphericalHarmonicsClear(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  lovrSphericalHarmonicsClear(sh);
  return 0;
}

static int l_lovrSphericalHarmonicsGetCoefficients(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  float coefficients[9][3];
  lovrSphericalHarmonicsGetCoefficients(sh, coefficients);
  lua_createtable(L, 9, 0);
  for (uint32_t i = 0; i < 9; i++) {
    lua_createtable(L, 3, 0);
    lua_pushnumber(L, coefficients[i][0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, coefficients[i][1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, coefficients[i][2]);
    lua_rawseti(L, -2, 3);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrSphericalHarmonicsSetCoefficients(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  luaL_checktype(L, 2, LUA_TTABLE);
  float coefficients[9][3];
  memset(coefficients, 0, sizeof(coefficients));
  int length = luax_len(L, 2);
  for (int i = 0; i < length; i++) {
    float color[4];
    lua_rawgeti(L, 2, i + 1);
    luax_optcolor(L, -1, color);
    memcpy(coefficients[i], color, 3 * sizeof(float));
    lua_pop(L, 1);
  }
  lovrSphericalHarmonicsSetCoefficients(sh, coefficients);
  return 0;
}

static int l_lovrSphericalHarmonicsEvaluate(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  float direction[4], color[4];
  luax_readvec3(L, 2, direction, NULL);
  lovrSphericalHarmonicsEvaluate(sh, direction, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  return 3;
}

static int l_lovrSphericalHarmonicsAddAmbientLight(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  float color[4];
  luax_readcolor(L, 2, color);
  lovrSphericalHarmonicsAddAmbientLight(sh, color);
  return 0;
}

static int l_lovrSphericalHarmonicsAddDirectionalLight(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  float direction[4];
  int index = luax_readvec3(L, 2, direction, NULL);
  float color[4];
  luax_readcolor(L, index, color);
  lovrSphericalHarmonicsAddDirectionalLight(sh, direction, color);
  return 0;
}

static int l_lovrSphericalHarmonicsAdd(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  SphericalHarmonics* other = luax_checktype(L, 2, SphericalHarmonics);
  lovrSphericalHarmonicsAdd(sh, other);
  return 0;
}

static int l_lovrSphericalHarmonicsLerp(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  SphericalHarmonics* other = luax_checktype(L, 2, SphericalHarmonics);
  float t = luax_checkfloat(L, 3);
  lovrSphericalHarmonicsLerp(sh, other, t);
  return 0;
}

static int l_lovrSphericalHarmonicsScale(lua_State* L) {
  SphericalHarmonics* sh = luax_checktype(L, 1, SphericalHarmonics);
  float scale = luax_checkfloat(L, 2);
  lovrSphericalHarmonicsScale(sh, scale);
  return 0;
}

const luaL_Reg lovrSphericalHarmonics[] = {
  { "clear", l_lovrSphericalHarmonicsClear },
  { "evaluate", l_lovrSphericalHarmonicsEvaluate },
  { "getCoefficients", l_lovrSphericalHarmonicsGetCoefficients },
  { "setCoefficients", l_lovrSphericalHarmonicsSetCoefficients },
  { "addAmbientLight", l_lovrSphericalHarmonicsAddAmbientLight },
  { "addDirectionalLight", l_lovrSphericalHarmonicsAddDirectionalLight },
  { "add", l_lovrSphericalHarmonicsAdd },
  { "lerp", l_lovrSphericalHarmonicsLerp },
  { "scale", l_lovrSphericalHarmonicsScale },
  { NULL, NULL }
};
