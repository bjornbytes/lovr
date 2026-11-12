#include "api.h"
#include "math/math.h"
#include "util.h"
#include <string.h>

void luax_readprobe(lua_State* L, int index, float* data) {
  if (lua_isnoneornil(L, index)) {
    memset(data, 0, 27 * sizeof(float));
  } else if (lua_type(L, index) == LUA_TNUMBER) {
    for (int i = 0; i < 27; i++) {
      data[i] = luax_tofloat(L, index + i);
    }
  } else if (lua_istable(L, index)) {
    lua_rawgeti(L, index, 1);
    int type = lua_type(L, -1);
    lua_pop(L, 1);

    if (type == LUA_TNUMBER) {
      for (int i = 0; i < 27; i++) {
        lua_rawgeti(L, index, i + 1);
        luax_check(L, lua_type(L, -1) == LUA_TNUMBER, "Expected table of 27 numbers");
        data[i] = luax_tofloat(L, -1);
        lua_pop(L, 1);
      }
    } else if (type == LUA_TTABLE) {
      for (int i = 0; i < 9; i++) {
        lua_rawgeti(L, index, i + 1);
        luax_check(L, lua_type(L, -1) == LUA_TTABLE, "Expected table of 9 tables");
        lua_rawgeti(L, -1, 1);
        if (lua_isnil(L, -1)) {
          lua_pop(L, 1);
          lua_getfield(L, -1, "x");
          lua_getfield(L, -2, "y");
          lua_getfield(L, -3, "z");
        } else {
          lua_rawgeti(L, -2, 2);
          lua_rawgeti(L, -3, 3);
        }
        data[3 * i + 0] = luax_tofloat(L, -3);
        data[3 * i + 1] = luax_tofloat(L, -2);
        data[3 * i + 2] = luax_tofloat(L, -1);
        lua_pop(L, 4);
      }
#ifdef LOVR_USE_LUAU
    } else if (type == LUA_TVECTOR) {
      for (int i = 0; i < 9; i++) {
        lua_rawgeti(L, index, i + 1);
        const float* v = lua_tovector(L, 1);
        luax_check(L, v, "Expected table of 9 vectors");
        vec3_init(data[3 * i], v);
        lua_pop(L, 1);
      }
#endif
    } else {
      luax_typeerror(L, index, "table of numbers, table of tables, or table of vectors");
    }
  } else {
    LightProbe* other = luax_totype(L, index, LightProbe);

    if (!other) {
      luax_typeerror(L, index, "number, table, or LightProbe");
      return;
    }

    memcpy(data, lovrLightProbeGetData(other), 27 * sizeof(float));
  }
}

static int l_lovrLightProbeClear(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  lovrLightProbeClear(probe);
  return 0;
}

static int l_lovrLightProbeSet(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  luax_readprobe(L, 2, lovrLightProbeGetData(probe));
  lua_settop(L, 1);
  return 1;
}

static int l_lovrLightProbeUnpack(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float* data = lovrLightProbeGetData(probe);
  for (uint32_t i = 0; i < 27; i++) {
    lua_pushnumber(L, data[i]);
  }
  return 27;
}

static int l_lovrLightProbeEvaluate(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float direction[3], color[3];
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
  lua_settop(L, 1);
  return 1;
}

static int l_lovrLightProbeAddDirectionalLight(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float direction[3];
  int index = luax_readvec3(L, 2, direction, NULL);
  float color[4];
  luax_readcolor(L, index, color);
  lovrLightProbeAddDirectionalLight(probe, direction, color);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrLightProbeAdd(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  LightProbe* other = luax_checktype(L, 2, LightProbe);
  float scale = luax_optfloat(L, 3, 1.f);
  lovrLightProbeAddProbe(probe, other, scale);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrLightProbeLerp(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  LightProbe* other = luax_checktype(L, 2, LightProbe);
  float t = luax_checkfloat(L, 3);
  lovrLightProbeLerp(probe, other, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrLightProbeScale(lua_State* L) {
  LightProbe* probe = luax_checktype(L, 1, LightProbe);
  float scale = luax_checkfloat(L, 2);
  lovrLightProbeScale(probe, scale);
  lua_settop(L, 1);
  return 1;
}

const luaL_Reg lovrLightProbe[] = {
  { "clear", l_lovrLightProbeClear },
  { "set", l_lovrLightProbeSet },
  { "unpack", l_lovrLightProbeUnpack },
  { "evaluate", l_lovrLightProbeEvaluate },
  { "addAmbientLight", l_lovrLightProbeAddAmbientLight },
  { "addDirectionalLight", l_lovrLightProbeAddDirectionalLight },
  { "add", l_lovrLightProbeAdd },
  { "lerp", l_lovrLightProbeLerp },
  { "scale", l_lovrLightProbeScale },
  { NULL, NULL }
};
