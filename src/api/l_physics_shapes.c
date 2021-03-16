#include "api.h"
#include "physics/physics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

void luax_pushshape(lua_State* L, Shape* shape) {
  switch (shape->type) {
    case SHAPE_SPHERE: luax_pushtype(L, SphereShape, shape); break;
    case SHAPE_BOX: luax_pushtype(L, BoxShape, shape); break;
    case SHAPE_CAPSULE: luax_pushtype(L, CapsuleShape, shape); break;
    case SHAPE_CYLINDER: luax_pushtype(L, CylinderShape, shape); break;
    case SHAPE_MESH: luax_pushtype(L, MeshShape, shape); break;
    default: lovrThrow("Unreachable");
  }
}

Shape* luax_checkshape(lua_State* L, int index) {
  Proxy* p = lua_touserdata(L, index);

  if (p) {
    const uint64_t hashes[] = {
      hash64("SphereShape", strlen("SphereShape")),
      hash64("BoxShape", strlen("BoxShape")),
      hash64("CapsuleShape", strlen("CapsuleShape")),
      hash64("CylinderShape", strlen("CylinderShape")),
      hash64("MeshShape", strlen("MeshShape")),
    };

    for (size_t i = 0; i < sizeof(hashes) / sizeof(hashes[0]); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  luax_typeerror(L, index, "Shape");
  return NULL;
}

static int l_lovrShapeDestroy(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrShapeDestroyData(shape);
  return 0;
}

static int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  luax_pushenum(L, ShapeType, lovrShapeGetType(shape));
  return 1;
}

static int l_lovrShapeGetCollider(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  luax_pushtype(L, Collider, lovrShapeGetCollider(shape));
  return 1;
}

static int l_lovrShapeIsEnabled(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lua_pushboolean(L, lovrShapeIsEnabled(shape));
  return 1;
}

static int l_lovrShapeSetEnabled(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  bool enabled = lua_toboolean(L, 2);
  lovrShapeSetEnabled(shape, enabled);
  return 0;
}

static int l_lovrShapeIsSensor(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lua_pushboolean(L, lovrShapeIsSensor(shape));
  return 1;
}

static int l_lovrShapeSetSensor(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  bool sensor = lua_toboolean(L, 2);
  lovrShapeSetSensor(shape, sensor);
  return 0;
}

static int l_lovrShapeGetUserData(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  union { int i; void* p; } ref = { .p = lovrShapeGetUserData(shape) };
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref.i);
  return 1;
}

static int l_lovrShapeSetUserData(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  union { int i; void* p; } ref = { .p = lovrShapeGetUserData(shape) };
  if (ref.i) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref.i);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref.i = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrShapeSetUserData(shape, ref.p);
  return 0;
}

static int l_lovrShapeGetPosition(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float x, y, z;
  lovrShapeGetPosition(shape, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrShapeSetPosition(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrAssert(lovrShapeGetCollider(shape) != NULL, "Shape must be attached to collider");
  float position[4];
  luax_readvec3(L, 2, position, NULL);
  lovrShapeSetPosition(shape, position[0], position[1], position[2]);
  return 0;
}

static int l_lovrShapeGetOrientation(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float angle, x, y, z, orientation[4];
  lovrShapeGetOrientation(shape, orientation);
  quat_getAngleAxis(orientation, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

static int l_lovrShapeSetOrientation(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrAssert(lovrShapeGetCollider(shape) != NULL, "Shape must be attached to collider");
  float orientation[4];
  luax_readquat(L, 2, orientation, NULL);
  lovrShapeSetOrientation(shape, orientation);
  return 0;
}

static int l_lovrShapeGetMass(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float density = luax_checkfloat(L, 2);
  float cx, cy, cz, mass;
  float inertia[6];
  lovrShapeGetMass(shape, density, &cx, &cy, &cz, &mass, inertia);
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

static int l_lovrShapeGetAABB(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float aabb[6];
  lovrShapeGetAABB(shape, aabb);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

#define lovrShape \
  { "destroy", l_lovrShapeDestroy }, \
  { "getType", l_lovrShapeGetType }, \
  { "getCollider", l_lovrShapeGetCollider }, \
  { "isEnabled", l_lovrShapeIsEnabled }, \
  { "setEnabled", l_lovrShapeSetEnabled }, \
  { "isSensor", l_lovrShapeIsSensor }, \
  { "setSensor", l_lovrShapeSetSensor }, \
  { "getUserData", l_lovrShapeGetUserData }, \
  { "setUserData", l_lovrShapeSetUserData }, \
  { "getPosition", l_lovrShapeGetPosition }, \
  { "setPosition", l_lovrShapeSetPosition }, \
  { "getOrientation", l_lovrShapeGetOrientation }, \
  { "setOrientation", l_lovrShapeSetOrientation }, \
  { "getMass", l_lovrShapeGetMass }, \
  { "getAABB", l_lovrShapeGetAABB }

static int l_lovrSphereShapeGetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  lua_pushnumber(L, lovrSphereShapeGetRadius(sphere));
  return 1;
}

static int l_lovrSphereShapeSetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  float radius = luax_checkfloat(L, 2);
  lovrSphereShapeSetRadius(sphere, radius);
  return 0;
}

const luaL_Reg lovrSphereShape[] = {
  lovrShape,
  { "getRadius", l_lovrSphereShapeGetRadius },
  { "setRadius", l_lovrSphereShapeSetRadius },
  { NULL, NULL }
};

static int l_lovrBoxShapeGetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float x, y, z;
  lovrBoxShapeGetDimensions(box, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrBoxShapeSetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float size[4];
  luax_readscale(L, 2, size, 3, NULL);
  lovrBoxShapeSetDimensions(box, size[0], size[1], size[2]);
  return 0;
}

const luaL_Reg lovrBoxShape[] = {
  lovrShape,
  { "getDimensions", l_lovrBoxShapeGetDimensions },
  { "setDimensions", l_lovrBoxShapeSetDimensions },
  { NULL, NULL }
};

static int l_lovrCapsuleShapeGetRadius(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetRadius(capsule));
  return 1;
}

static int l_lovrCapsuleShapeSetRadius(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  float radius = luax_checkfloat(L, 2);
  lovrCapsuleShapeSetRadius(capsule, radius);
  return 0;
}

static int l_lovrCapsuleShapeGetLength(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetLength(capsule));
  return 1;
}

static int l_lovrCapsuleShapeSetLength(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  float length = luax_checkfloat(L, 2);
  lovrCapsuleShapeSetLength(capsule, length);
  return 0;
}

const luaL_Reg lovrCapsuleShape[] = {
  lovrShape,
  { "getRadius", l_lovrCapsuleShapeGetRadius },
  { "setRadius", l_lovrCapsuleShapeSetRadius },
  { "getLength", l_lovrCapsuleShapeGetLength },
  { "setLength", l_lovrCapsuleShapeSetLength },
  { NULL, NULL }
};

static int l_lovrCylinderShapeGetRadius(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetRadius(cylinder));
  return 1;
}

static int l_lovrCylinderShapeSetRadius(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  float radius = luax_checkfloat(L, 2);
  lovrCylinderShapeSetRadius(cylinder, radius);
  return 0;
}

static int l_lovrCylinderShapeGetLength(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetLength(cylinder));
  return 1;
}

static int l_lovrCylinderShapeSetLength(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  float length = luax_checkfloat(L, 2);
  lovrCylinderShapeSetLength(cylinder, length);
  return 0;
}

const luaL_Reg lovrCylinderShape[] = {
  lovrShape,
  { "getRadius", l_lovrCylinderShapeGetRadius },
  { "setRadius", l_lovrCylinderShapeSetRadius },
  { "getLength", l_lovrCylinderShapeGetLength },
  { "setLength", l_lovrCylinderShapeSetLength },
  { NULL, NULL }
};

const luaL_Reg lovrMeshShape[] = {
  lovrShape,
  { NULL, NULL }
};
