#include "api.h"
#include "headset/headset.h"
#include "core/maf.h"
#include "util.h"

static int l_lovrAnchorIsActive(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  bool active = lovrAnchorIsActive(anchor);
  lua_pushboolean(L, active);
  return 1;
}

static int l_lovrAnchorIsTracked(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  bool tracked = lovrAnchorIsTracked(anchor);
  lua_pushboolean(L, tracked);
  return 1;
}

static int l_lovrAnchorGetParent(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  Anchor* parent = lovrAnchorGetParent(anchor);
  luax_pushtype(L, Anchor, parent);
  return 1;
}

static int l_lovrAnchorGetPosition(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float position[3];
  lovrAnchorGetPosition(anchor, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrAnchorGetOrientation(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float orientation[4], angle, ax, ay, az;
  lovrAnchorGetOrientation(anchor, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrAnchorGetPose(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrAnchorGetPosition(anchor, position);
  lovrAnchorGetOrientation(anchor, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrAnchorGetWidth(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[0]);
  return 1;
}

static int l_lovrAnchorGetHeight(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[1]);
  return 1;
}

static int l_lovrAnchorGetDepth(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[2]);
  return 1;
}

static int l_lovrAnchorGetDimensions(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[0]);
  lua_pushnumber(L, dimensions[1]);
  lua_pushnumber(L, dimensions[2]);
  return 3;
}

static int l_lovrAnchorGetLabel(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);
  size_t length;
  const char* label = lovrAnchorGetLabel(anchor, &length);
  lua_pushlstring(L, label, length);
  return 1;
}

static int l_lovrAnchorGetTriangles(lua_State* L) {
  Anchor* anchor = luax_checktype(L, 1, Anchor);

  uint32_t vertexCount;
  const float* vertices = lovrAnchorGetVertices(anchor, &vertexCount);

  uint32_t indexCount;
  const uint32_t* indices = lovrAnchorGetIndices(anchor, &indexCount);

  if (!vertices || !indices) {
    lua_pushnil(L);
    return 1;
  }

  lua_createtable(L, (int) vertexCount, 0);
  for (uint32_t i = 0; i < 3 * vertexCount; i++) {
    lua_pushnumber(L, vertices[i]);
    lua_rawseti(L, -2, (int) i + 1);
  }

  lua_createtable(L, (int) indexCount, 0);
  for (uint32_t i = 0; i < indexCount; i++) {
    lua_pushinteger(L, indices[i] + 1);
    lua_rawseti(L, -2, (int) i + 1);
  }

  return 2;
}

const luaL_Reg lovrAnchor[] = {
  { "isActive", l_lovrAnchorIsActive },
  { "isTracked", l_lovrAnchorIsTracked },
  { "getParent", l_lovrAnchorGetParent },
  { "getLabel", l_lovrAnchorGetLabel },
  { "getPosition", l_lovrAnchorGetPosition },
  { "getOrientation", l_lovrAnchorGetOrientation },
  { "getPose", l_lovrAnchorGetPose },
  { "getWidth", l_lovrAnchorGetWidth },
  { "getHeight", l_lovrAnchorGetHeight },
  { "getDepth", l_lovrAnchorGetDepth },
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "getTriangles", l_lovrAnchorGetTriangles },
  { NULL, NULL }
};
