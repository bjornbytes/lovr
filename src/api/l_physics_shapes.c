#include "api.h"
#include "physics/physics.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

void luax_pushshape(lua_State* L, Shape* shape) {
  switch (lovrShapeGetType(shape)) {
    case SHAPE_SPHERE: luax_pushtype(L, SphereShape, shape); break;
    case SHAPE_BOX: luax_pushtype(L, BoxShape, shape); break;
    case SHAPE_CAPSULE: luax_pushtype(L, CapsuleShape, shape); break;
    case SHAPE_CYLINDER: luax_pushtype(L, CylinderShape, shape); break;
    case SHAPE_MESH: luax_pushtype(L, MeshShape, shape); break;
    case SHAPE_TERRAIN: luax_pushtype(L, TerrainShape, shape); break;
    default: lovrUnreachable();
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
      hash64("TerrainShape", strlen("TerrainShape"))
    };

    for (size_t i = 0; i < COUNTOF(hashes); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  luax_typeerror(L, index, "Shape");
  return NULL;
}

Shape* luax_newsphereshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index, 1.f);
  return lovrSphereShapeCreate(radius);
}

Shape* luax_newboxshape(lua_State* L, int index) {
  float size[3];
  luax_readscale(L, index, size, 3, NULL);
  return lovrBoxShapeCreate(size[0], size[1], size[2]);
}

Shape* luax_newcapsuleshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  return lovrCapsuleShapeCreate(radius, length);
}

Shape* luax_newcylindershape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  return lovrCylinderShapeCreate(radius, length);
}

Shape* luax_newmeshshape(lua_State* L, int index) {
  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount;
  uint32_t indexCount;
  bool shouldFree;

  luax_readmesh(L, index, &vertices, &vertexCount, &indices, &indexCount, &shouldFree);

  // If we do not own the mesh data, we must make a copy
  // ode's trimesh collider needs to own the triangle info for the lifetime of the geom
  // Note that if shouldFree is true, we don't free the data and let the physics module do it when
  // the collider/shape is destroyed
  if (!shouldFree) {
    float* v = vertices;
    uint32_t* i = indices;
    vertices = lovrMalloc(3 * vertexCount * sizeof(float));
    indices = lovrMalloc(indexCount * sizeof(uint32_t));
    memcpy(vertices, v, 3 * vertexCount * sizeof(float));
    memcpy(indices, i, indexCount * sizeof(uint32_t));
  }

  return lovrMeshShapeCreate(vertexCount, vertices, indexCount, indices);
}

Shape* luax_newterrainshape(lua_State* L, int index) {
  float scaleXZ = luax_checkfloat(L, index++);
  int type = lua_type(L, index);
  if (type == LUA_TNIL || type == LUA_TNONE) {
    float vertices[4] = { 0.f };
    return lovrTerrainShapeCreate(vertices, 2, scaleXZ, 1.f);
  } else if (type == LUA_TFUNCTION) {
    uint32_t n = luax_optu32(L, index + 1, 100);
    float* vertices = lovrMalloc(sizeof(float) * n * n);
    for (uint32_t i = 0; i < n * n; i++) {
      float x = scaleXZ * (-.5f + ((float) (i % n)) / n);
      float z = scaleXZ * (-.5f + ((float) (i / n)) / n);
      lua_pushvalue(L, index);
      lua_pushnumber(L, x);
      lua_pushnumber(L, z);
      lua_call(L, 2, 1);
      lovrCheck(lua_type(L, -1) == LUA_TNUMBER, "Expected TerrainShape callback to return a number");
      vertices[i] = luax_tofloat(L, -1);
      lua_pop(L, 1);
    }
    TerrainShape* shape = lovrTerrainShapeCreate(vertices, n, scaleXZ, 1.f);
    lovrFree(vertices);
    return shape;
  } else if (type == LUA_TUSERDATA) {
    Image* image = luax_checktype(L, index, Image);
    uint32_t n = lovrImageGetWidth(image, 0);
    lovrCheck(lovrImageGetHeight(image, 0) == n, "TerrainShape images must be square");
    float scaleY = luax_optfloat(L, index + 1, 1.f);
    float* vertices = lovrMalloc(sizeof(float) * n * n);
    for (uint32_t y = 0; y < n; y++) {
      for (uint32_t x = 0; x < n; x++) {
        float pixel[4];
        lovrImageGetPixel(image, x, y, pixel);
        vertices[x + y * n] = pixel[0];
      }
    }
    TerrainShape* shape = lovrTerrainShapeCreate(vertices, n, scaleXZ, scaleY);
    lovrFree(vertices);
    return shape;
  } else {
    luax_typeerror(L, index, "Image, number, or function");
    return NULL;
  }
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

static void luax_pushshapestash(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrshapestash");

  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // metatable
    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrshapestash");
  }
}

static int l_lovrShapeGetUserData(lua_State* L) {
  luax_checktype(L, 1, Shape);
  luax_pushshapestash(L);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrShapeSetUserData(lua_State* L) {
  luax_checktype(L, 1, Shape);
  luax_pushshapestash(L);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_rawset(L, -3);
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
  lovrCheck(lovrShapeGetCollider(shape) != NULL, "Shape must be attached to collider");
  float position[3];
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
  lovrCheck(lovrShapeGetCollider(shape) != NULL, "Shape must be attached to collider");
  float orientation[4];
  luax_readquat(L, 2, orientation, NULL);
  lovrShapeSetOrientation(shape, orientation);
  return 0;
}

static int l_lovrShapeGetPose(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float x, y, z;
  lovrShapeGetPosition(shape, &x, &y, &z);
  float angle, ax, ay, az, orientation[4];
  lovrShapeGetOrientation(shape, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrShapeSetPose(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrCheck(lovrShapeGetCollider(shape) != NULL, "Shape must be attached to collider");
  float position[3], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readquat(L, index, orientation, NULL);
  lovrShapeSetPosition(shape, position[0], position[1], position[2]);
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
  { "getPose", l_lovrShapeGetPose }, \
  { "setPose", l_lovrShapeSetPose }, \
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
  float w, h, d;
  lovrBoxShapeGetDimensions(box, &w, &h, &d);
  lua_pushnumber(L, w);
  lua_pushnumber(L, h);
  lua_pushnumber(L, d);
  return 3;
}

static int l_lovrBoxShapeSetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float size[3];
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

const luaL_Reg lovrTerrainShape[] = {
  lovrShape,
  { NULL, NULL }
};
