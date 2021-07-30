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
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  lua_pushinteger(L, info->views);
  return 1;
}

static int l_lovrCanvasGetClear(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  float color[4][4];
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
  if (info->depth.format) {
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
  float color[4][4];
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
  { "getClear", l_lovrCanvasGetClear },
  { "setClear", l_lovrCanvasSetClear },
  { NULL, NULL }
};
