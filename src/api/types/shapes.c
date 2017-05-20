#include "api/lovr.h"
#include "physics/physics.h"

int luax_pushshape(lua_State* L, Shape* shape) {
  switch (lovrShapeGetType(shape)) {
    case SHAPE_SPHERE: luax_pushtype(L, SphereShape, shape); return 1;
    case SHAPE_BOX: luax_pushtype(L, BoxShape, shape); return 1;
    case SHAPE_CAPSULE: luax_pushtype(L, CapsuleShape, shape); return 1;
    case SHAPE_CYLINDER: luax_pushtype(L, CylinderShape, shape); return 1;
  }
}

int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  luax_pushenum(L, &ShapeTypes, lovrShapeGetType(shape));
  return 1;
}

int l_lovrShapeGetCollider(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  luax_pushtype(L, Collider, lovrShapeGetCollider(shape));
  return 1;
}

int l_lovrShapeIsEnabled(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  lua_pushboolean(L, lovrShapeIsEnabled(shape));
  return 1;
}

int l_lovrShapeSetEnabled(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  int enabled = lua_toboolean(L, 2);
  lovrShapeSetEnabled(shape, enabled);
  return 0;
}

int l_lovrShapeGetUserData(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  int ref = (int) lovrShapeGetUserData(shape);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  return 1;
}

int l_lovrShapeSetUserData(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint64_t ref = (int) lovrShapeGetUserData(shape);
  if (ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrShapeSetUserData(shape, (void*) ref);
  return 0;
}

int l_lovrShapeGetPosition(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  float x, y, z;
  lovrShapeGetPosition(shape, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrShapeSetPosition(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrShapeSetPosition(shape, x, y, z);
  return 0;
}

int l_lovrShapeGetOrientation(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  float angle, x, y, z;
  lovrShapeGetOrientation(shape, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrShapeSetOrientation(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  float angle = luaL_checknumber(L, 2);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  lovrShapeSetOrientation(shape, angle, x, y, z);
  return 0;
}

int l_lovrShapeGetCategory(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint32_t category = lovrShapeGetCategory(shape);
  int count = 0;

  for (int i = 0; i < 32; i++) {
    if (category & (1 << i)) {
      lua_pushinteger(L, i + 1);
      count++;
    }
  }

  return count;
}

int l_lovrShapeSetCategory(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint32_t category = 0;

  for (int i = 2; i <= lua_gettop(L); i++) {
    category |= (1 << luaL_checkinteger(L, i));
  }

  lovrShapeSetCategory(shape, category);
  return 0;
}

int l_lovrShapeGetMask(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint32_t mask = lovrShapeGetMask(shape);
  int count = 0;

  for (int i = 0; i < 32; i++) {
    if (mask & (1 << i)) {
      lua_pushinteger(L, i + 1);
      count++;
    }
  }

  return count;
}

int l_lovrShapeSetMask(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  uint32_t mask = 0;

  for (int i = 2; i <= lua_gettop(L); i++) {
    mask |= (1 << luaL_checkinteger(L, i));
  }

  lovrShapeSetMask(shape, ~mask);
  return 0;
}

int l_lovrShapeComputeMass(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  float density = luaL_checknumber(L, 2);
  float cx, cy, cz, mass;
  float inertia[6];
  lovrShapeComputeMass(shape, density, &cx, &cy, &cz, &mass, inertia);
  lua_pushnumber(L, cx);
  lua_pushnumber(L, cy);
  lua_pushnumber(L, cz);
  lua_pushnumber(L, mass);
  lua_newtable(L);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, inertia[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 5;
}

const luaL_Reg lovrShape[] = {
  { "getType", l_lovrShapeGetType },
  { "getCollider", l_lovrShapeGetCollider },
  { "isEnabled", l_lovrShapeIsEnabled },
  { "setEnabled", l_lovrShapeSetEnabled },
  { "getUserData", l_lovrShapeGetUserData },
  { "setUserData", l_lovrShapeSetUserData },
  { "getPosition", l_lovrShapeGetPosition },
  { "setPosition", l_lovrShapeSetPosition },
  { "getOrientation", l_lovrShapeGetOrientation },
  { "setOrientation", l_lovrShapeSetOrientation },
  { "getCategory", l_lovrShapeGetCategory },
  { "setCategory", l_lovrShapeSetCategory },
  { "getMask", l_lovrShapeGetMask },
  { "setMask", l_lovrShapeSetMask },
  { "computeMass", l_lovrShapeComputeMass },
  { NULL, NULL }
};

int l_lovrSphereShapeGetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  lua_pushnumber(L, lovrSphereShapeGetRadius(sphere));
  return 1;
}

int l_lovrSphereShapeSetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  float radius = luaL_checknumber(L, 2);
  lovrSphereShapeSetRadius(sphere, radius);
  return 0;
}

const luaL_Reg lovrSphereShape[] = {
  { "getRadius", l_lovrSphereShapeGetRadius },
  { "setRadius", l_lovrSphereShapeSetRadius },
  { NULL, NULL }
};

int l_lovrBoxShapeGetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float x, y, z;
  lovrBoxShapeGetDimensions(box, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBoxShapeSetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBoxShapeSetDimensions(box, x, y, z);
  return 0;
}

const luaL_Reg lovrBoxShape[] = {
  { "getDimensions", l_lovrBoxShapeGetDimensions },
  { "setDimensions", l_lovrBoxShapeSetDimensions },
  { NULL, NULL }
};

int l_lovrCapsuleShapeGetRadius(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetRadius(capsule));
  return 1;
}

int l_lovrCapsuleShapeSetRadius(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  float radius = luaL_checknumber(L, 2);
  lovrCapsuleShapeSetRadius(capsule, radius);
  return 0;
}

int l_lovrCapsuleShapeGetLength(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetLength(capsule));
  return 1;
}

int l_lovrCapsuleShapeSetLength(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  float length = luaL_checknumber(L, 2);
  lovrCapsuleShapeSetLength(capsule, length);
  return 0;
}

const luaL_Reg lovrCapsuleShape[] = {
  { "getRadius", l_lovrCapsuleShapeGetRadius },
  { "setRadius", l_lovrCapsuleShapeSetRadius },
  { "getLength", l_lovrCapsuleShapeGetLength },
  { "setLength", l_lovrCapsuleShapeSetLength },
  { NULL, NULL }
};

int l_lovrCylinderShapeGetRadius(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetRadius(cylinder));
  return 1;
}

int l_lovrCylinderShapeSetRadius(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  float radius = luaL_checknumber(L, 2);
  lovrCylinderShapeSetRadius(cylinder, radius);
  return 0;
}

int l_lovrCylinderShapeGetLength(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetLength(cylinder));
  return 1;
}

int l_lovrCylinderShapeSetLength(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  float length = luaL_checknumber(L, 2);
  lovrCylinderShapeSetLength(cylinder, length);
  return 0;
}

const luaL_Reg lovrCylinderShape[] = {
  { "getRadius", l_lovrCylinderShapeGetRadius },
  { "setRadius", l_lovrCylinderShapeSetRadius },
  { "getLength", l_lovrCylinderShapeGetLength },
  { "setLength", l_lovrCylinderShapeSetLength },
  { NULL, NULL }
};
