#include "api/api.h"
#include "graphics/graphics.h"
#include "data/image.h"
#include "core/maf.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static int l_lovrCanvasGetWidth(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t width = lovrCanvasGetWidth(canvas);
  lua_pushinteger(L, width);
  return 1;
}

static int l_lovrCanvasGetHeight(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t height = lovrCanvasGetHeight(canvas);
  lua_pushinteger(L, height);
  return 1;
}

static int l_lovrCanvasGetDimensions(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t width = lovrCanvasGetWidth(canvas);
  uint32_t height = lovrCanvasGetHeight(canvas);
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static int l_lovrCanvasGetSampleCount(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  lua_pushinteger(L, info->samples);
  return 1;
}

static int l_lovrCanvasGetViewCount(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t views = lovrCanvasGetViewCount(canvas);
  lua_pushinteger(L, views);
  return 1;
}

static int l_lovrCanvasGetViewPose(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 4);
    lovrCanvasGetViewMatrix(canvas, view, matrix);
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    lovrCanvasGetViewMatrix(canvas, view, matrix);
    mat4_invert(matrix);
    mat4_getAngleAxis(matrix, &angle, &ax, &ay, &az);
    lua_pushnumber(L, matrix[12]);
    lua_pushnumber(L, matrix[13]);
    lua_pushnumber(L, matrix[14]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 7;
  }
}

static int l_lovrCanvasSetViewPose(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  VectorType type;
  float* p = luax_tovector(L, 3, &type);
  if (p && type == V_MAT4) {
    float matrix[16];
    mat4_init(matrix, p);
    bool inverted = lua_toboolean(L, 3);
    if (!inverted) mat4_invert(matrix);
    lovrCanvasSetViewMatrix(canvas, view, matrix);
  } else {
    int index = 3;
    float position[4], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromQuat(matrix, orientation);
    memcpy(matrix + 12, position, 3 * sizeof(float));
    mat4_invert(matrix);
    lovrCanvasSetViewMatrix(canvas, view, matrix);
  }
  return 0;
}

static int l_lovrCanvasGetProjection(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    lovrCanvasGetProjection(canvas, view, matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    lovrCanvasGetProjection(canvas, view, matrix);
    mat4_getFov(matrix, &left, &right, &up, &down);
    lua_pushnumber(L, left);
    lua_pushnumber(L, right);
    lua_pushnumber(L, up);
    lua_pushnumber(L, down);
    return 4;
  }
}

static int l_lovrCanvasSetProjection(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_type(L, 3) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 3);
    float right = luax_checkfloat(L, 4);
    float up = luax_checkfloat(L, 5);
    float down = luax_checkfloat(L, 6);
    float clipNear = luax_optfloat(L, 7, .1f);
    float clipFar = luax_optfloat(L, 8, 100.f);
    float matrix[16];
    mat4_fov(matrix, left, right, up, down, clipNear, clipFar);
    lovrCanvasSetProjection(canvas, view, matrix);
  } else {
    float* matrix = luax_checkvector(L, 2, V_MAT4, "mat4 or number");
    lovrCanvasSetProjection(canvas, view, matrix);
  }
  return 0;
}

static int l_lovrCanvasGetClear(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  float color[MAX_COLOR_ATTACHMENTS][4];
  float depth;
  uint8_t stencil;
  lovrCanvasGetClear(canvas, color, &depth, &stencil);
  lua_createtable(L, info->count, 2);
  for (uint32_t i = 0; i < info->count; i++) {
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, color[i][0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, color[i][1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, color[i][2]);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, color[i][3]);
    lua_rawseti(L, -2, 4);
    lua_rawseti(L, -2, i + 1);
  }
  if (info->depth.enabled) {
    lua_pushnumber(L, depth);
    lua_setfield(L, -2, "depth");
    if (info->depth.format == FORMAT_D24S8) {
      lua_pushinteger(L, stencil);
      lua_setfield(L, -2, "stencil");
    }
  }
  return 1;
}

static int l_lovrCanvasSetClear(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  float color[MAX_COLOR_ATTACHMENTS][4];
  float depth;
  uint8_t stencil;
  lovrCanvasGetClear(canvas, color, &depth, &stencil);
  if (lua_istable(L, 2)) {
    for (uint32_t i = 0; i < info->count; i++) {
      lua_rawgeti(L, 2, i + 1);
      if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        lua_rawgeti(L, -3, 3);
        lua_rawgeti(L, -4, 4);
        luax_readcolor(L, -4, color[i]);
        lua_pop(L, 4);
      } else {
        luax_readcolor(L, -1, color[i]);
      }
      lua_pop(L, 1);
    }
    lua_getfield(L, 2, "depth");
    depth = luax_optfloat(L, -1, depth);
    lua_getfield(L, 2, "stencil");
    stencil = luaL_optinteger(L, -1, stencil);
    lua_pop(L, 2);
  } else {
    for (uint32_t i = 0; i < info->count; i++) {
      if (lua_istable(L, i + 1)) {
        luax_readcolor(L, i + 1, color[i]);
      } else {
        lua_pushvalue(L, i + 1);
        luax_readcolor(L, -1, color[i]);
        lua_pop(L, 1);
      }
    }
  }
  lovrCanvasSetClear(canvas, color, depth, stencil);
  return 0;
}

const luaL_Reg lovrCanvas[] = {
  { "getWidth", l_lovrCanvasGetWidth },
  { "getHeight", l_lovrCanvasGetHeight },
  { "getDimensions", l_lovrCanvasGetDimensions },
  { "getSampleCount", l_lovrCanvasGetSampleCount },
  { "getViewCount", l_lovrCanvasGetViewCount },
  { "getViewPose", l_lovrCanvasGetViewPose },
  { "setViewPose", l_lovrCanvasSetViewPose },
  { "getProjection", l_lovrCanvasGetProjection },
  { "setProjection", l_lovrCanvasSetProjection },
  { "getClear", l_lovrCanvasGetClear },
  { "setClear", l_lovrCanvasSetClear },
  { NULL, NULL }
};
