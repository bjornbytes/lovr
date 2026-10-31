#include "api.h"
#include "headset/headset.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include <stdlib.h>
#include "util.h"

void luax_pushanchor(lua_State* L, Anchor* anchor) {
  if (!anchor) {
    lua_pushnil(L);
    return;
  }

  _luax_pushtype(L, T_PointAnchor + lovrAnchorGetType(anchor), anchor);
}

Anchor* luax_toanchor(lua_State* L, int index) {
  Object* object = lua_touserdata(L, index);

  if (!object || object->type < T_PointAnchor || object->type > T_MeshAnchor) {
    return NULL;
  }

  return object->pointer;
}

Anchor* luax_checkanchor(lua_State* L, int index) {
  Anchor* anchor = luax_toanchor(L, index);
  if (!anchor) return luax_typeerror(L, index, "Anchor"), NULL;
  return anchor;
}

static int l_lovrAnchorIsActive(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  bool active = lovrAnchorIsActive(anchor);
  lua_pushboolean(L, active);
  return 1;
}

static int l_lovrAnchorIsTracked(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  bool tracked = lovrAnchorIsTracked(anchor);
  lua_pushboolean(L, tracked);
  return 1;
}

static int l_lovrAnchorGetPosition(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  float position[3];
  lovrAnchorGetPosition(anchor, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrAnchorGetOrientation(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
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
  Anchor* anchor = luax_checkanchor(L, 1);
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
  Anchor* anchor = luax_checkanchor(L, 1);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[0]);
  return 1;
}

static int l_lovrAnchorGetHeight(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[1]);
  return 1;
}

static int l_lovrAnchorGetDepth(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  float dimensions[3];
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[2]);
  return 1;
}

static int l_lovrAnchorGetDimensions(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  float dimensions[3];
  int n = lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[0]);
  lua_pushnumber(L, dimensions[1]);
  if (n == 3) lua_pushnumber(L, dimensions[2]);
  return n;
}

static int l_lovrAnchorGetLabel(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  size_t length;
  const char* label = lovrAnchorGetLabel(anchor, &length);
  lua_pushlstring(L, label, length);
  return 1;
}

static int l_lovrAnchorDidMeshChange(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  bool changed = lovrAnchorDidMeshChange(anchor);
  lua_pushboolean(L, changed);
  return 1;
}

static int l_lovrAnchorGetMesh(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
#ifdef LOVR_DISABLE_GRAPHICS
  lua_pushnil(L);
#else
  Mesh* mesh = lovrAnchorGetMesh(anchor);
  luax_pushtype(L, Mesh, mesh);
#endif
  return 1;
}

static int l_lovrAnchorGetMeshPose(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrAnchorGetMeshPose(anchor, position, orientation);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrAnchorGetVertexCount(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t count;
  lovrAnchorGetVertices(anchor, &count, NULL);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrAnchorGetIndexCount(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t count;
  lovrAnchorGetIndices(anchor, &count);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrAnchorGetVertex2D(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  uint32_t count;
  float* vertices = lovrAnchorGetVertices(anchor, &count, NULL);
  lovrCheck(index < count, "Invalid vertex index %d", index + 1);
  lua_pushnumber(L, vertices[index * 2 + 0]);
  lua_pushnumber(L, vertices[index * 2 + 1]);
  return 2;
}

static int l_lovrAnchorGetIndexU16(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  uint32_t count;
  uint16_t* indices = lovrAnchorGetIndices(anchor, &count);
  lovrCheck(index < count, "Invalid index %d", index + 1);
  lua_pushinteger(L, indices[index]);
  return 1;
}

static int l_lovrAnchorGetVertex3D(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  uint32_t count;
  float* normals;
  float* vertices = lovrAnchorGetVertices(anchor, &count, &normals);
  lovrCheck(index < count, "Invalid vertex index %d", index + 1);
  lua_pushnumber(L, vertices[index * 3 + 0]);
  lua_pushnumber(L, vertices[index * 3 + 1]);
  lua_pushnumber(L, vertices[index * 3 + 2]);
  lua_pushnumber(L, normals ? normals[index * 3 + 0] : 0.);
  lua_pushnumber(L, normals ? normals[index * 3 + 1] : 0.);
  lua_pushnumber(L, normals ? normals[index * 3 + 2] : 0.);
  return 6;
}

static int l_lovrAnchorGetIndexU32(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  uint32_t count;
  uint32_t* indices = lovrAnchorGetIndices(anchor, &count);
  lovrCheck(index < count, "Invalid index %d", index + 1);
  lua_pushinteger(L, indices[index]);
  return 1;
}

static int l_lovrAnchorSave(lua_State* L) {
  PointAnchor* anchor = luax_checktype(L, 1, PointAnchor);
  return luax_yieldfuture(L, lovrPointAnchorSave(anchor), NULL);
}

static int l_lovrAnchorDelete(lua_State* L) {
  PointAnchor* anchor = luax_checktype(L, 1, PointAnchor);
  return luax_yieldfuture(L, lovrPointAnchorDelete(anchor), NULL);
}

#define lovrAnchor \
  { "isActive", l_lovrAnchorIsActive }, \
  { "isTracked", l_lovrAnchorIsTracked }, \
  { "getPosition", l_lovrAnchorGetPosition }, \
  { "getOrientation", l_lovrAnchorGetOrientation }, \
  { "getPose", l_lovrAnchorGetPose }

const luaL_Reg lovrPointAnchor[] = {
  lovrAnchor,
  { "getLabel", l_lovrAnchorGetLabel },
  { "save", l_lovrAnchorSave },
  { "delete", l_lovrAnchorDelete },
  { NULL, NULL }
};

const luaL_Reg lovrPlaneAnchor[] = {
  lovrAnchor,
  { "getWidth", l_lovrAnchorGetWidth },
  { "getHeight", l_lovrAnchorGetHeight },
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "getLabel", l_lovrAnchorGetLabel },
  { "didMeshChange", l_lovrAnchorDidMeshChange },
  { "getMeshPose", l_lovrAnchorGetMeshPose },
  { "getVertexCount", l_lovrAnchorGetVertexCount },
  { "getIndexCount", l_lovrAnchorGetIndexCount },
  { "getVertex", l_lovrAnchorGetVertex2D },
  { "getIndex", l_lovrAnchorGetIndexU16 },
  { NULL, NULL }
};

const luaL_Reg lovrMarkerAnchor[] = {
  lovrAnchor,
  { "getWidth", l_lovrAnchorGetWidth },
  { "getHeight", l_lovrAnchorGetHeight },
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "getLabel", l_lovrAnchorGetLabel },
  { NULL, NULL }
};

const luaL_Reg lovrImageAnchor[] = {
  lovrAnchor,
  { "getWidth", l_lovrAnchorGetWidth },
  { "getHeight", l_lovrAnchorGetHeight },
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "getLabel", l_lovrAnchorGetLabel },
  { NULL, NULL }
};

const luaL_Reg lovrObjectAnchor[] = {
  lovrAnchor,
  { "getWidth", l_lovrAnchorGetWidth },
  { "getHeight", l_lovrAnchorGetHeight },
  { "getDepth", l_lovrAnchorGetDepth },
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "getLabel", l_lovrAnchorGetLabel },
  { NULL, NULL }
};

const luaL_Reg lovrMeshAnchor[] = {
  lovrAnchor,
  { "getWidth", l_lovrAnchorGetWidth }, \
  { "getHeight", l_lovrAnchorGetHeight }, \
  { "getDepth", l_lovrAnchorGetDepth }, \
  { "getDimensions", l_lovrAnchorGetDimensions },
  { "didMeshChange", l_lovrAnchorDidMeshChange },
  { "getMeshPose", l_lovrAnchorGetMeshPose },
  { "getVertexCount", l_lovrAnchorGetVertexCount },
  { "getIndexCount", l_lovrAnchorGetIndexCount },
  { "getVertex", l_lovrAnchorGetVertex3D },
  { "getIndex", l_lovrAnchorGetIndexU32 },
  { NULL, NULL }
};
