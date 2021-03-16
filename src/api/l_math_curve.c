#include "api.h"
#include "math/curve.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrCurveEvaluate(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t = luax_checkfloat(L, 2);
  float point[4];
  lovrCurveEvaluate(curve, t, point);
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
  if (lovrCurveGetPointCount(curve) == 2) {
    n = 2;
  }
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
  luax_pushtype(L, Curve, subcurve);
  return 1;
}

static int l_lovrCurveGetPointCount(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  lua_pushinteger(L, lovrCurveGetPointCount(curve));
  return 1;
}

static int l_lovrCurveGetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  size_t index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[4];
  lovrCurveGetPoint(curve, index, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

static int l_lovrCurveSetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  size_t index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[4];
  luax_readvec3(L, 3, point, NULL);
  lovrCurveSetPoint(curve, index, point);
  return 0;
}

static int l_lovrCurveAddPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float point[4];
  int i = luax_readvec3(L, 2, point, NULL);
  size_t index = lua_isnoneornil(L, i) ? lovrCurveGetPointCount(curve) : (size_t) luaL_checkinteger(L, i) - 1;
  lovrAssert(index <= lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveAddPoint(curve, point, index);
  return 0;
}

static int l_lovrCurveRemovePoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  size_t index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveRemovePoint(curve, index);
  return 0;
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
  { NULL, NULL }
};
