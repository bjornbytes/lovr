#include "api.h"
#include "util.h"
#include <string.h>

static int l_lovrLightProbeClear(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  lovrLightProbeClear(probe);
  return 0;
}

static int l_lovrLightProbeGetCoefficients(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float coefficients[9][3];
  lovrLightProbeGetCoefficients(probe, coefficients);
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

static int l_lovrLightProbeSetCoefficients(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
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
  lovrLightProbeSetCoefficients(probe, coefficients);
  return 0;
}

static int l_lovrLightProbeEvaluate(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float direction[4], color[4];
  luax_readvec3(L, 2, direction, NULL);
  lovrLightProbeEvaluate(probe, direction, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  return 3;
}

static int l_lovrLightProbeAddAmbientLight(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float color[4];
  luax_readcolor(L, 2, color);
  lovrLightProbeAddAmbientLight(probe, color);
  return 0;
}

static int l_lovrLightProbeAddDirectionalLight(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float direction[4];
  int index = luax_readvec3(L, 2, direction, NULL);
  float color[4];
  luax_readcolor(L, index, color);
  lovrLightProbeAddDirectionalLight(probe, direction, color);
  return 0;
}

static int l_lovrLightProbeAdd(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  LightProbe* other = luax_checktype(L, 2, LightProbe);
  lovrLightProbeAddProbe(probe, other);
  return 0;
}

static int l_lovrLightProbeLerp(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  LightProbe* other = luax_checktype(L, 2, LightProbe);
  float t = luax_checkfloat(L, 3);
  lovrLightProbeLerp(probe, other, t);
  return 0;
}

static int l_lovrLightProbeScale(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float scale = luax_checkfloat(L, 2);
  lovrLightProbeScale(probe, scale);
  return 0;
}

const luaL_Reg lovrLightProbe[] = {
  { "clear", l_lovrLightProbeClear },
  { "evaluate", l_lovrLightProbeEvaluate },
  { "getCoefficients", l_lovrLightProbeGetCoefficients },
  { "setCoefficients", l_lovrLightProbeSetCoefficients },
  { "addAmbientLight", l_lovrLightProbeAddAmbientLight },
  { "addDirectionalLight", l_lovrLightProbeAddDirectionalLight },
  { "add", l_lovrLightProbeAdd },
  { "lerp", l_lovrLightProbeLerp },
  { "scale", l_lovrLightProbeScale },
  { NULL, NULL }
};
