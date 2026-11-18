#include "api.h"
#include "math/math.h"
#include "util.h"

static int l_lovrCurveEvaluate(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t = luax_checkfloat(L, 2);
  float point[4];
  luax_assert(L, lovrCurveEvaluate(curve, t, point));
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

static int l_lovrCurveGetTangent(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t = luax_checkfloat(L, 2);
  float point[4];
  lovrCurveGetTangent(curve, t, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

static int l_lovrCurveRender(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  int n = luaL_optinteger(L, 2, 32);
  float t1 = luax_optfloat(L, 3, 0.);
  float t2 = luax_optfloat(L, 4, 1.);
  lua_createtable(L, n * 3, 0);
  float step = 1.f / (n - 1);
  for (int i = 0; i < n; i++) {
    float point[4];
    lovrCurveEvaluate(curve, t1 + (t2 - t1) * i * step, point);
    lua_pushnumber(L, point[0]);
    lua_rawseti(L, -2, 3 * i + 1);
    lua_pushnumber(L, point[1]);
    lua_rawseti(L, -2, 3 * i + 2);
    lua_pushnumber(L, point[2]);
    lua_rawseti(L, -2, 3 * i + 3);
  }
  return 1;
}

static int l_lovrCurveSlice(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t1 = luax_checkfloat(L, 2);
  float t2 = luax_checkfloat(L, 3);
  Curve* subcurve = lovrCurveSlice(curve, t1, t2);
  luax_assert(L, subcurve);
  luax_pushtype(L, Curve, subcurve);
  lovrRelease(subcurve, lovrCurveDestroy);
  return 1;
}

static int l_lovrCurveGetPointCount(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  lua_pushinteger(L, lovrCurveGetPointCount(curve));
  return 1;
}

static int l_lovrCurveGetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[4];
  lovrCurveGetPoint(curve, index, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

static int l_lovrCurveSetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[4];
  luax_readvec3(L, 3, point, NULL);
  lovrCurveSetPoint(curve, index, point);
  return 0;
}

static int l_lovrCurveAddPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float point[4];
  int i = luax_readvec3(L, 2, point, NULL);
  uint32_t index = lua_isnoneornil(L, i) ? lovrCurveGetPointCount(curve) : luax_checku32(L, i) - 1;
  luax_check(L, index <= lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveAddPoint(curve, point, index);
  return 0;
}

static int l_lovrCurveRemovePoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveRemovePoint(curve, index);
  return 0;
}

static int l_lovrCurveGetLength(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t = luaL_optnumber(L, 2, 1.f);
  uint32_t iterations = luax_optu32(L, 3, 16);
  float length = lovrCurveGetLength(curve, t, iterations);
  lua_pushnumber(L, length);
  return 1;
}

static int l_lovrCurveStep(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float distance = luax_checkfloat(L, 2);
  uint32_t iterations = luax_optu32(L, 3, 16);
  float t = lovrCurveStep(curve, distance, iterations);
  lua_pushnumber(L, t);
  return 1;
}

const luaL_Reg lovrCurve[] = {
  { "evaluate", l_lovrCurveEvaluate },
  { "getTangent", l_lovrCurveGetTangent },
  { "render", l_lovrCurveRender },
  { "slice", l_lovrCurveSlice },
  { "getPointCount", l_lovrCurveGetPointCount },
  { "getPoint", l_lovrCurveGetPoint },
  { "setPoint", l_lovrCurveSetPoint },
  { "addPoint", l_lovrCurveAddPoint },
  { "removePoint", l_lovrCurveRemovePoint },
  { "getLength", l_lovrCurveGetLength },
  { "step", l_lovrCurveStep },
  { NULL, NULL }
};
