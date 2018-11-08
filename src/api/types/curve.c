#include "api.h"
#include "math/curve.h"

int l_lovrCurveEvaluate(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t = luaL_checknumber(L, 2);
  float point[3];
  lovrCurveEvaluate(curve, t, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

int l_lovrCurveRender(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  int n = luaL_optinteger(L, 2, 32);
  float t1 = luaL_optnumber(L, 3, 0.f);
  float t2 = luaL_optnumber(L, 4, 1.f);
  float* points = malloc(3 * n * sizeof(float));
  lovrCurveRender(curve, t1, t2, points, n);
  lua_createtable(L, n, 0);
  for (int i = 0; i < 3 * n; i++) {
    lua_pushnumber(L, points[i]);
    lua_rawseti(L, -2, i + 1);
  }
  free(points);
  return 1;
}

int l_lovrCurveSplit(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float t1 = luaL_checknumber(L, 2);
  float t2 = luaL_checknumber(L, 3);
  Curve* subcurve = lovrCurveSplit(curve, t1, t2);
  luax_pushobject(L, subcurve);
  return 1;
}

int l_lovrCurveGetPointCount(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  lua_pushinteger(L, lovrCurveGetPointCount(curve));
  return 1;
}

int l_lovrCurveGetDegree(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  lua_pushinteger(L, lovrCurveGetPointCount(curve) - 1);
  return 1;
}

int l_lovrCurveGetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  int index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index >= 0 && index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[3];
  lovrCurveGetPoint(curve, index, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

int l_lovrCurveSetPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  int index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index >= 0 && index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  float point[3] = { luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5) };
  lovrCurveSetPoint(curve, index, point);
  return 0;
}

int l_lovrCurveAddPoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  float point[3] = { luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4) };
  int index = lua_isnoneornil(L, 5) ? lovrCurveGetPointCount(curve) : luaL_checkinteger(L, 5) - 1;
  lovrAssert(index >= 0 && index <= lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveAddPoint(curve, point, index);
  return 0;
}

int l_lovrCurveRemovePoint(lua_State* L) {
  Curve* curve = luax_checktype(L, 1, Curve);
  int index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index >= 0 && index < lovrCurveGetPointCount(curve), "Invalid Curve point index: %d", index + 1);
  lovrCurveRemovePoint(curve, index);
  return 0;
}

const luaL_Reg lovrCurve[] = {
  { "evaluate", l_lovrCurveEvaluate },
  { "render", l_lovrCurveRender },
  { "split", l_lovrCurveSplit },
  { "getPointCount", l_lovrCurveGetPointCount },
  { "getDegree", l_lovrCurveGetDegree },
  { "getPoint", l_lovrCurveGetPoint },
  { "setPoint", l_lovrCurveSetPoint },
  { "addPoint", l_lovrCurveAddPoint },
  { "removePoint", l_lovrCurveRemovePoint },
  { NULL, NULL }
};
