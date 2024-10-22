#include "api.h"
#include "physics/physics.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

void luax_pushshape(lua_State* L, Shape* shape) {
  switch (lovrShapeGetType(shape)) {
    case SHAPE_BOX: luax_pushtype(L, BoxShape, shape); break;
    case SHAPE_SPHERE: luax_pushtype(L, SphereShape, shape); break;
    case SHAPE_CAPSULE: luax_pushtype(L, CapsuleShape, shape); break;
    case SHAPE_CYLINDER: luax_pushtype(L, CylinderShape, shape); break;
    case SHAPE_CONVEX: luax_pushtype(L, ConvexShape, shape); break;
    case SHAPE_MESH: luax_pushtype(L, MeshShape, shape); break;
    case SHAPE_TERRAIN: luax_pushtype(L, TerrainShape, shape); break;
    default: lovrUnreachable();
  }
}

static Shape* luax_toshape(lua_State* L, int index) {
  Proxy* p = lua_touserdata(L, index);

  if (p) {
    const uint64_t hashes[] = {
      hash64("BoxShape", strlen("BoxShape")),
      hash64("SphereShape", strlen("SphereShape")),
      hash64("CapsuleShape", strlen("CapsuleShape")),
      hash64("CylinderShape", strlen("CylinderShape")),
      hash64("ConvexShape", strlen("ConvexShape")),
      hash64("MeshShape", strlen("MeshShape")),
      hash64("TerrainShape", strlen("TerrainShape"))
    };

    for (size_t i = 0; i < COUNTOF(hashes); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  return NULL;
}

Shape* luax_checkshape(lua_State* L, int index) {
  Shape* shape = luax_toshape(L, index);
  if (shape) {
    luax_check(L, !lovrShapeIsDestroyed(shape), "Attempt to use a destroyed Shape");
    return shape;
  } else {
    luax_typeerror(L, index, "Shape");
    return NULL;
  }
}

Shape* luax_newboxshape(lua_State* L, int index) {
  float size[3];
  luax_readscale(L, index, size, 3, NULL);
  Shape* shape = lovrBoxShapeCreate(size);
  luax_assert(L, shape);
  return shape;
}

Shape* luax_newsphereshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index, 1.f);
  Shape* shape = lovrSphereShapeCreate(radius);
  luax_assert(L, shape);
  return shape;
}

Shape* luax_newcapsuleshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  Shape* shape = lovrCapsuleShapeCreate(radius, length);
  luax_assert(L, shape);
  return shape;
}

Shape* luax_newcylindershape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  Shape* shape = lovrCylinderShapeCreate(radius, length);
  luax_assert(L, shape);
  return shape;
}

Shape* luax_newconvexshape(lua_State* L, int index) {
  ConvexShape* parent = luax_totype(L, index, ConvexShape);

  if (parent) {
    float scale = luax_optfloat(L, index + 1, 1.f);
    return lovrConvexShapeClone(parent, scale);
  }

  float* points;
  uint32_t count;
  bool shouldFree;
  index = luax_readmesh(L, index, &points, &count, NULL, NULL, &shouldFree);
  float scale = luax_optfloat(L, index, 1.f);
  ConvexShape* shape = lovrConvexShapeCreate(points, count, scale);
  if (shouldFree) lovrFree(points);
  luax_assert(L, shape);
  return shape;
}

Shape* luax_newmeshshape(lua_State* L, int index) {
  MeshShape* parent = luax_totype(L, index, MeshShape);

  if (parent) {
    float scale = luax_optfloat(L, index + 1, 1.f);
    return lovrMeshShapeClone(parent, scale);
  }

  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount;
  uint32_t indexCount;
  bool shouldFree;

  index = luax_readmesh(L, index, &vertices, &vertexCount, &indices, &indexCount, &shouldFree);

  float scale = luax_optfloat(L, index, 1.f);
  Shape* shape = lovrMeshShapeCreate(vertexCount, vertices, indexCount, indices, scale);

  if (shouldFree) {
    lovrFree(vertices);
    lovrFree(indices);
  }

  luax_assert(L, shape);
  return shape;
}

Shape* luax_newterrainshape(lua_State* L, int index) {
  float scaleXZ = luax_checkfloat(L, index++);
  int type = lua_type(L, index);
  if (type == LUA_TNIL || type == LUA_TNONE) {
    float vertices[9] = { 0.f };
    Shape* shape = lovrTerrainShapeCreate(vertices, 3, scaleXZ, 1.f);
    luax_assert(L, shape);
    return shape;
  } else if (type == LUA_TFUNCTION) {
    uint32_t n = luax_optu32(L, index + 1, 100);
    float* vertices = lovrMalloc(sizeof(float) * n * n);
    for (uint32_t i = 0; i < n * n; i++) {
      float x = scaleXZ * (-.5f + ((float) (i % n)) / (n - 1));
      float z = scaleXZ * (-.5f + ((float) (i / n)) / (n - 1));
      lua_pushvalue(L, index);
      lua_pushnumber(L, x);
      lua_pushnumber(L, z);
      lua_call(L, 2, 1);
      luax_check(L, lua_type(L, -1) == LUA_TNUMBER, "Expected TerrainShape callback to return a number");
      vertices[i] = luax_tofloat(L, -1);
      lua_pop(L, 1);
    }
    TerrainShape* shape = lovrTerrainShapeCreate(vertices, n, scaleXZ, 1.f);
    lovrFree(vertices);
    luax_assert(L, shape);
    return shape;
  } else if (type == LUA_TUSERDATA) {
    Image* image = luax_checktype(L, index, Image);
    uint32_t n = lovrImageGetWidth(image, 0);
    luax_check(L, lovrImageGetHeight(image, 0) == n, "TerrainShape images must be square");
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
    luax_assert(L, shape);
    return shape;
  } else {
    luax_typeerror(L, index, "nil, Image, or function");
    return NULL;
  }
}

static int l_lovrShapeDestroy(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrShapeDestruct(shape);
  return 0;
}

static int l_lovrShapeIsDestroyed(lua_State* L) {
  Shape* shape = luax_toshape(L, 1);
  if (!shape) luax_typeerror(L, 1, "Shape");
  bool destroyed = lovrShapeIsDestroyed(shape);
  lua_pushboolean(L, destroyed);
  return 1;
}

static int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  luax_pushenum(L, ShapeType, lovrShapeGetType(shape));
  return 1;
}

static int l_lovrShapeGetCollider(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  Collider* collider = lovrShapeGetCollider(shape);
  luax_pushtype(L, Collider, collider);
  return 1;
}

static int l_lovrShapeGetUserData(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lua_pushlightuserdata(L, shape);
  lua_rawget(L, LUA_REGISTRYINDEX);
  return 1;
}

static int l_lovrShapeSetUserData(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lovrShapeSetUserData(shape, (uintptr_t) lua_tothread(L, -1));
  lua_pushlightuserdata(L, shape);
  lua_pushvalue(L, 2);
  lua_rawset(L, LUA_REGISTRYINDEX);
  return 0;
}

static int l_lovrShapeGetVolume(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float volume = lovrShapeGetVolume(shape);
  lua_pushnumber(L, volume);
  return 1;
}

static int l_lovrShapeGetDensity(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float density = lovrShapeGetDensity(shape);
  lua_pushnumber(L, density);
  return 1;
}

static int l_lovrShapeSetDensity(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float density = luax_checkfloat(L, 2);
  lovrShapeSetDensity(shape, density);
  return 0;
}

static int l_lovrShapeGetMass(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float mass = lovrShapeGetMass(shape);
  lua_pushnumber(L, mass);
  return 1;
}

static int l_lovrShapeGetInertia(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float diagonal[3], rotation[4], angle, ax, ay, az;
  lovrShapeGetInertia(shape, diagonal, rotation);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, diagonal[0]);
  lua_pushnumber(L, diagonal[1]);
  lua_pushnumber(L, diagonal[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrShapeGetCenterOfMass(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float center[3];
  lovrShapeGetCenterOfMass(shape, center);
  lua_pushnumber(L, center[0]);
  lua_pushnumber(L, center[1]);
  lua_pushnumber(L, center[2]);
  return 3;
}

static int l_lovrShapeGetOffset(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrShapeGetOffset(shape, position, orientation);
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

static int l_lovrShapeSetOffset(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float position[3], orientation[4];
  int index = 2;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  luax_assert(L, lovrShapeSetOffset(shape, position, orientation));
  return 0;
}

static int l_lovrShapeGetPosition(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float position[3];
  lovrShapeGetPose(shape, position, NULL);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrShapeGetOrientation(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float orientation[4], angle, x, y, z;
  lovrShapeGetPose(shape, NULL, orientation);
  quat_getAngleAxis(orientation, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

static int l_lovrShapeGetPose(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrShapeGetPose(shape, position, orientation);
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

static int l_lovrShapeGetAABB(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float aabb[6];
  lovrShapeGetAABB(shape, aabb);
  for (int i = 0; i < 6; i++) lua_pushnumber(L, aabb[i]);
  return 6;
}

static int l_lovrShapeContainsPoint(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float point[3];
  luax_readvec3(L, 2, point, NULL);
  bool hit = lovrShapeContainsPoint(shape, point);
  lua_pushboolean(L, hit);
  return 1;
}

static int l_lovrShapeRaycast(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  int index = 2;
  float start[3], end[3];
  index = luax_readvec3(L, index, start, NULL);
  index = luax_readvec3(L, index, end, NULL);
  CastResult hit;
  if (lovrShapeRaycast(shape, start, end, &hit)) {
    lua_pushnumber(L, hit.position[0]);
    lua_pushnumber(L, hit.position[1]);
    lua_pushnumber(L, hit.position[2]);
    lua_pushnumber(L, hit.normal[0]);
    lua_pushnumber(L, hit.normal[1]);
    lua_pushnumber(L, hit.normal[2]);
    if (hit.triangle == ~0u) {
      lua_pushnil(L);
    } else {
      lua_pushinteger(L, hit.triangle + 1);
    }
    return 7;
  }
  return 0;
}

#define lovrShape \
  { "destroy", l_lovrShapeDestroy }, \
  { "isDestroyed", l_lovrShapeIsDestroyed }, \
  { "getType", l_lovrShapeGetType }, \
  { "getCollider", l_lovrShapeGetCollider }, \
  { "getUserData", l_lovrShapeGetUserData }, \
  { "setUserData", l_lovrShapeSetUserData }, \
  { "getVolume", l_lovrShapeGetVolume }, \
  { "getDensity", l_lovrShapeGetDensity }, \
  { "setDensity", l_lovrShapeSetDensity }, \
  { "getMass", l_lovrShapeGetMass }, \
  { "getInertia", l_lovrShapeGetInertia }, \
  { "getCenterOfMass", l_lovrShapeGetCenterOfMass }, \
  { "getOffset", l_lovrShapeGetOffset }, \
  { "setOffset", l_lovrShapeSetOffset }, \
  { "getPosition", l_lovrShapeGetPosition }, \
  { "getOrientation", l_lovrShapeGetOrientation }, \
  { "getPose", l_lovrShapeGetPose }, \
  { "getAABB", l_lovrShapeGetAABB }, \
  { "containsPoint", l_lovrShapeContainsPoint }, \
  { "raycast", l_lovrShapeRaycast }

static int l_lovrBoxShapeGetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float dimensions[3];
  lovrBoxShapeGetDimensions(box, dimensions);
  lua_pushnumber(L, dimensions[0]);
  lua_pushnumber(L, dimensions[1]);
  lua_pushnumber(L, dimensions[2]);
  return 3;
}

static int l_lovrBoxShapeSetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float dimensions[3];
  luax_readvec3(L, 2, dimensions, NULL);
  luax_assert(L, lovrBoxShapeSetDimensions(box, dimensions));
  return 0;
}

const luaL_Reg lovrBoxShape[] = {
  lovrShape,
  { "getDimensions", l_lovrBoxShapeGetDimensions },
  { "setDimensions", l_lovrBoxShapeSetDimensions },
  { NULL, NULL }
};

static int l_lovrSphereShapeGetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  lua_pushnumber(L, lovrSphereShapeGetRadius(sphere));
  return 1;
}

static int l_lovrSphereShapeSetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  float radius = luax_checkfloat(L, 2);
  luax_assert(L, lovrSphereShapeSetRadius(sphere, radius));
  return 0;
}

const luaL_Reg lovrSphereShape[] = {
  lovrShape,
  { "getRadius", l_lovrSphereShapeGetRadius },
  { "setRadius", l_lovrSphereShapeSetRadius },
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
  luax_assert(L, lovrCapsuleShapeSetRadius(capsule, radius));
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
  luax_assert(L, lovrCapsuleShapeSetLength(capsule, length));
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
  luax_assert(L, lovrCylinderShapeSetRadius(cylinder, radius));
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
  luax_assert(L, lovrCylinderShapeSetLength(cylinder, length));
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

static int l_lovrConvexShapeGetPointCount(lua_State* L) {
  ConvexShape* convex = luax_checktype(L, 1, ConvexShape);
  uint32_t count = lovrConvexShapeGetPointCount(convex);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrConvexShapeGetPoint(lua_State* L) {
  ConvexShape* convex = luax_checktype(L, 1, ConvexShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  float point[3];
  luax_assert(L, lovrConvexShapeGetPoint(convex, index, point));
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

static int l_lovrConvexShapeGetFaceCount(lua_State* L) {
  ConvexShape* convex = luax_checktype(L, 1, ConvexShape);
  uint32_t count = lovrConvexShapeGetFaceCount(convex);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrConvexShapeGetFace(lua_State* L) {
  ConvexShape* convex = luax_checktype(L, 1, ConvexShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  uint32_t count = lovrConvexShapeGetFace(convex, index, NULL, 0);
  luax_assert(L, count);
  lua_createtable(L, (int) count, 0);
  uint32_t stack[8];
  uint32_t* indices = count > COUNTOF(stack) ? lovrMalloc(count * sizeof(uint32_t)) : stack;
  lovrConvexShapeGetFace(convex, index, indices, count);
  for (uint32_t i = 0; i < count; i++) {
    lua_pushinteger(L, indices[i]);
    lua_rawseti(L, -2, i + 1);
  }
  if (indices != stack) {
    lovrFree(indices);
  }
  return 1;
}

static int l_lovrConvexShapeGetScale(lua_State* L) {
  ConvexShape* convex = luax_checktype(L, 1, ConvexShape);
  float scale = lovrConvexShapeGetScale(convex);
  lua_pushnumber(L, scale);
  return 1;
}

const luaL_Reg lovrConvexShape[] = {
  lovrShape,
  { "getPointCount", l_lovrConvexShapeGetPointCount },
  { "getPoint", l_lovrConvexShapeGetPoint },
  { "getFaceCount", l_lovrConvexShapeGetFaceCount },
  { "getFace", l_lovrConvexShapeGetFace },
  { "getScale", l_lovrConvexShapeGetScale },
  { NULL, NULL }
};

static int l_lovrMeshShapeGetScale(lua_State* L) {
  MeshShape* mesh = luax_checktype(L, 1, MeshShape);
  float scale = lovrMeshShapeGetScale(mesh);
  lua_pushnumber(L, scale);
  return 1;
}

const luaL_Reg lovrMeshShape[] = {
  lovrShape,
  { "getScale", l_lovrMeshShapeGetScale },
  { NULL, NULL }
};

const luaL_Reg lovrTerrainShape[] = {
  lovrShape,
  { NULL, NULL }
};
