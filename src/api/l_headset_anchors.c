#include "api.h"
#include "headset/headset.h"
#include "core/maf.h"
#include <stdlib.h>
#include "util.h"

void luax_pushanchor(lua_State* L, Anchor* anchor) {
  if (anchor) {
    switch (lovrAnchorGetType(anchor)) {
      case ANCHOR_POINT: luax_pushtype(L, PointAnchor, anchor); break;
      case ANCHOR_PLANE: luax_pushtype(L, PlaneAnchor, anchor); break;
      case ANCHOR_MARKER: luax_pushtype(L, MarkerAnchor, anchor); break;
      default: lovrUnreachable();
    }
  } else {
    lua_pushnil(L);
  }
}

Anchor* luax_toanchor(lua_State* L, int index) {
  Object* object = lua_touserdata(L, index);

  if (!object) {
    return NULL;
  }

  switch (object->type) {
    case T_PointAnchor: return object->pointer;
    case T_PlaneAnchor: return object->pointer;
    case T_MarkerAnchor: return object->pointer;
    default: return NULL;
  }
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

static int l_lovrAnchorGetParent(lua_State* L) {
  Anchor* anchor = luax_checkanchor(L, 1);
  Anchor* parent = lovrAnchorGetParent(anchor);
  luax_pushanchor(L, parent);
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
  lovrAnchorGetDimensions(anchor, dimensions);
  lua_pushnumber(L, dimensions[0]);
  lua_pushnumber(L, dimensions[1]);
  lua_pushnumber(L, dimensions[2]);
  return 3;
}

#define lovrAnchor \
  { "isActive", l_lovrAnchorIsActive }, \
  { "isTracked", l_lovrAnchorIsTracked }, \
  { "getParent", l_lovrAnchorGetParent }, \
  { "getPosition", l_lovrAnchorGetPosition }, \
  { "getOrientation", l_lovrAnchorGetOrientation }, \
  { "getPose", l_lovrAnchorGetPose }, \
  { "getWidth", l_lovrAnchorGetWidth }, \
  { "getHeight", l_lovrAnchorGetHeight }, \
  { "getDepth", l_lovrAnchorGetDepth }, \
  { "getDimensions", l_lovrAnchorGetDimensions }

static int l_lovrPointAnchorGetID(lua_State* L) {
  PointAnchor* anchor = luax_checktype(L, 1, PointAnchor);

  size_t length;
  const uint8_t* id = lovrPointAnchorGetID(anchor, &length);
  if (!id) {
    lua_pushnil(L);
    return 1;
  }

  char hex[32];
  size_t cursor = 0;
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = id[i];
    for (int j = 0; j < 2; j++) {
      uint8_t nibble = j == 0 ? (byte >> 4) & 0xf : byte & 0xf;
      if (nibble >= 0xa) {
        hex[cursor++] = 'a' + (nibble - 0xa);
      } else {
        hex[cursor++] = '0' + nibble;
      }
    }
  }

  lua_pushlstring(L, hex, cursor);
  return 1;
}

static int l_lovrPointAnchorSave(lua_State* L) {
  PointAnchor* anchor = luax_checktype(L, 1, PointAnchor);
  Future* future = lovrPointAnchorSave(anchor);
  luax_assert(L, future);
  return luax_yieldfuture(L, future, NULL);
}

static int l_lovrPointAnchorDelete(lua_State* L) {
  PointAnchor* anchor = luax_checktype(L, 1, PointAnchor);
  Future* future = lovrPointAnchorDelete(anchor);
  luax_assert(L, future);
  return luax_yieldfuture(L, future, NULL);
}

const luaL_Reg lovrPointAnchor[] = {
  lovrAnchor,
  { "getID", l_lovrPointAnchorGetID },
  { "save", l_lovrPointAnchorSave },
  { "delete", l_lovrPointAnchorDelete },
  { NULL, NULL }
};

static int l_lovrPlaneAnchorGetLabel(lua_State* L) {
  PlaneAnchor* anchor = luax_checktype(L, 1, PlaneAnchor);
  size_t length;
  const char* label = lovrPlaneAnchorGetLabel(anchor, &length);
  lua_pushlstring(L, label, length);
  return 1;
}

static int l_lovrPlaneAnchorGetTriangles(lua_State* L) {
  PlaneAnchor* anchor = luax_checktype(L, 1, PlaneAnchor);

  uint32_t vertexCount;
  const float* vertices = lovrPlaneAnchorGetVertices(anchor, &vertexCount);

  uint32_t indexCount;
  const uint32_t* indices = lovrPlaneAnchorGetIndices(anchor, &indexCount);

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

const luaL_Reg lovrPlaneAnchor[] = {
  lovrAnchor,
  { "getLabel", l_lovrPlaneAnchorGetLabel },
  { "getTriangles", l_lovrPlaneAnchorGetTriangles },
  { NULL, NULL }
};

static int l_lovrMarkerAnchorGetLabel(lua_State* L) {
  MarkerAnchor* anchor = luax_checktype(L, 1, MarkerAnchor);
  size_t length;
  const char* label = lovrMarkerAnchorGetLabel(anchor, &length);
  lua_pushlstring(L, label, length);
  return 1;
}

const luaL_Reg lovrMarkerAnchor[] = {
  lovrAnchor,
  { "getLabel", l_lovrMarkerAnchorGetLabel },
  { NULL, NULL }
};
